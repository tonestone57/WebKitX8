/*
 * Copyright (C) 2014 Haiku, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SourceBufferPrivateHaiku.h"

#if ENABLE(MEDIA_SOURCE)

#include "MediaSourcePrivateHaiku.h"
#include <wtf/Logger.h>
#include <DataIO.h>
#include <OS.h>
#include <vector>
#include <algorithm>

namespace WebCore {

class SourceBufferAdapterIO : public BDataIO {
public:
    SourceBufferAdapterIO()
        : m_lock("SourceBufferAdapterIO Lock")
        , m_notifyRead(create_sem(0, "SourceBufferAdapterIO Read"))
        , m_isEnded(false)
        , m_isAborted(false)
        , m_position(0)
    {
    }

    virtual ~SourceBufferAdapterIO()
    {
        Abort();
        delete_sem(m_notifyRead);
    }

    void Abort()
    {
        m_lock.Lock();
        m_isAborted = true;
        m_lock.Unlock();
        // Wake up any waiting readers
        release_sem(m_notifyRead);
    }

    ssize_t Read(void* buffer, size_t size) override
    {
        while (true) {
            m_lock.Lock();

            if (m_isAborted) {
                m_lock.Unlock();
                return B_ERROR; // Or B_INTERRUPTED
            }

            if (m_position < m_data.size()) {
                size_t bytesToRead = std::min(size, m_data.size() - m_position);
                memcpy(buffer, m_data.data() + m_position, bytesToRead);
                m_position += bytesToRead;
                m_lock.Unlock();
                return bytesToRead;
            }

            if (m_isEnded) {
                m_lock.Unlock();
                return 0; // EOF
            }

            m_lock.Unlock();

            // Wait for data
            status_t err = acquire_sem(m_notifyRead);
            if (err != B_OK) return err;
        }
    }

    ssize_t Write(const void* buffer, size_t size) override
    {
        return B_NOT_ALLOWED;
    }

    // Custom append
    void AppendData(const void* data, size_t size)
    {
        if (size == 0) return;

        m_lock.Lock();
        // Check abort?
        if (m_isAborted) {
            m_lock.Unlock();
            return;
        }

        // FIXME: Unbounded memory growth.
        // We accumulate all data in memory because BMediaFile seeks around.
        // To fix this, we need to implement a smarter buffering strategy that
        // caches enough for BMediaFile or implements a file-backed buffer.
        size_t currentSize = m_data.size();
        m_data.resize(currentSize + size);
        memcpy(m_data.data() + currentSize, data, size);
        m_lock.Unlock();

        // Signal reader
        release_sem(m_notifyRead);
    }

    void SignalEnded()
    {
        m_lock.Lock();
        m_isEnded = true;
        m_lock.Unlock();
        release_sem(m_notifyRead);
    }

    off_t Seek(off_t position, uint32 seekMode) override
    {
        m_lock.Lock();
        switch (seekMode) {
        case SEEK_SET:
            m_position = position;
            break;
        case SEEK_CUR:
            m_position += position;
            break;
        case SEEK_END:
            m_position = m_data.size() + position;
            break;
        }

        if (m_position < 0) m_position = 0;

        m_lock.Unlock();
        return m_position;
    }

    off_t Position() const override
    {
        return m_position;
    }

private:
    BLocker m_lock;
    sem_id m_notifyRead;
    bool m_isEnded;
    bool m_isAborted;
    std::vector<uint8_t> m_data;
    off_t m_position;
};

Ref<SourceBufferPrivateHaiku> SourceBufferPrivateHaiku::create(MediaSourcePrivateHaiku& mediaSource, const ContentType& contentType)
{
    return adoptRef(*new SourceBufferPrivateHaiku(mediaSource, contentType));
}

SourceBufferPrivateHaiku::SourceBufferPrivateHaiku(MediaSourcePrivateHaiku& mediaSource, const ContentType& contentType)
    : SourceBufferPrivate(mediaSource)
    , m_mediaSource(mediaSource)
    , m_contentType(contentType)
    , m_stream(new SourceBufferAdapterIO())
#if !RELEASE_LOG_DISABLED
    , m_logger(mediaSource.logger())
    , m_logIdentifier(Logger::nextLogIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);
}

SourceBufferPrivateHaiku::~SourceBufferPrivateHaiku()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_stream->Abort();
    delete m_stream;
    m_mediaSource.sourceBufferPrivateDidClose(this);
}

void SourceBufferPrivateHaiku::abort()
{
    SourceBufferPrivate::abort();
    if (m_stream) m_stream->Abort();
}

Ref<MediaPromise> SourceBufferPrivateHaiku::appendInternal(Ref<SharedBuffer>&& data)
{
    ALWAYS_LOG(LOGIDENTIFIER, "data size: ", data->size());

    if (data->isEmpty())
        return MediaPromise::createAndResolve();

    // Note: Data accumulation is enabled for testing the BMediaFile integration path.
    // In production, buffer size limits should be enforced.
    // FIXME: This will leak memory for long streams. See SourceBufferAdapterIO.
    for (const auto& segment : *data) {
        m_stream->AppendData(segment.data(), segment.size());
    }

    return MediaPromise::createAndResolve();
}

void SourceBufferPrivateHaiku::resetParserStateInternal()
{
    ALWAYS_LOG(LOGIDENTIFIER);
}

void SourceBufferPrivateHaiku::removedFromMediaSource()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (m_stream) m_stream->Abort();
    SourceBufferPrivate::removedFromMediaSource();
}

// Helper to access the stream from outside
BDataIO* SourceBufferPrivateHaiku::dataIO() const
{
    return m_stream;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)
