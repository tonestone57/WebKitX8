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

#ifndef SourceBufferPrivateHaiku_h
#define SourceBufferPrivateHaiku_h

#if ENABLE(MEDIA_SOURCE)

#include "ContentType.h"
#include "SourceBufferPrivate.h"
#include <wtf/Vector.h>
#include <wtf/SharedBuffer.h>

class BDataIO;

namespace WebCore {

class MediaSourcePrivateHaiku;
class SourceBufferAdapterIO;

class SourceBufferPrivateHaiku final : public SourceBufferPrivate {
public:
    static Ref<SourceBufferPrivateHaiku> create(MediaSourcePrivateHaiku&, const ContentType&);
    virtual ~SourceBufferPrivateHaiku();

    constexpr MediaPlatformType platformType() const override { return MediaPlatformType::Haiku; }

    void removedFromMediaSource() override;
    void abort() override;

    // For MediaPlayerPrivateHaiku to read
    BDataIO* dataIO() const;

#if !RELEASE_LOG_DISABLED
    const Logger& sourceBufferLogger() const override { return m_logger; }
    uint64_t sourceBufferLogIdentifier() override { return m_logIdentifier; }
#endif

private:
    SourceBufferPrivateHaiku(MediaSourcePrivateHaiku&, const ContentType&);

    Ref<MediaPromise> appendInternal(Ref<SharedBuffer>&&) override;
    void resetParserStateInternal() override;

    MediaSourcePrivateHaiku& m_mediaSource;
    ContentType m_contentType;
    SourceBufferAdapterIO* m_stream; // Owned

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    uint64_t m_logIdentifier;
#endif
};

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)

#endif // SourceBufferPrivateHaiku_h
