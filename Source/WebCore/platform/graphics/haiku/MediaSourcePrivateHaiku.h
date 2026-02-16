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

#ifndef MediaSourcePrivateHaiku_h
#define MediaSourcePrivateHaiku_h

#if ENABLE(MEDIA_SOURCE)

#include "MediaSourcePrivate.h"
#include <wtf/LoggerHelper.h>
#include <wtf/WeakPtr.h>
#include <wtf/HashSet.h>

namespace WebCore {

class MediaPlayerPrivate;
class SourceBufferPrivateHaiku;

class MediaSourcePrivateHaiku final
    : public MediaSourcePrivate
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    explicit MediaSourcePrivateHaiku(MediaPlayerPrivate&, MediaSourcePrivateClient&);
    virtual ~MediaSourcePrivateHaiku();

    AddStatus addSourceBuffer(const ContentType&, RefPtr<SourceBufferPrivate>&) override;
    void removeSourceBuffer(SourceBufferPrivate&) override;
    void durationChanged(const MediaTime&) override;
    void markEndOfStream(EndOfStreamStatus) override;
    void unmarkEndOfStream() override;
    bool isEnded() const override;

    MediaPlayer::ReadyState readyState() const override;
    void setReadyState(MediaPlayer::ReadyState) override;

    void waitForSeekCompleted(float, const MediaTime&, Promise&&) override;
    void seekToTime(const MediaTime&) override;

    // Called by SourceBufferPrivateHaiku destructor
    void sourceBufferPrivateDidClose(SourceBufferPrivateHaiku*);

    void abortAllSourceBuffers();

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const override { return m_logger; }
    const void* logIdentifier() const override { return m_logIdentifier; }
    const char* logClassName() const override { return "MediaSourcePrivateHaiku"; }
    WTFLogChannel& logChannel() const override;
#endif

private:
    MediaPlayerPrivate& m_player;
    Ref<MediaSourcePrivateClient> m_client;
    MediaPlayer::ReadyState m_readyState { MediaPlayer::ReadyState::HaveNothing };
    bool m_isEnded { false };

    // Track active source buffers to abort them
    HashSet<SourceBufferPrivateHaiku*> m_sourceBuffers;

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif
};

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)

#endif // MediaSourcePrivateHaiku_h
