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
#include "MediaSourcePrivateHaiku.h"

#if ENABLE(MEDIA_SOURCE)

#include "MediaPlayerPrivateHaiku.h"
#include "SourceBufferPrivateHaiku.h"
#include <wtf/Logger.h>

namespace WebCore {

MediaSourcePrivateHaiku::MediaSourcePrivateHaiku(MediaPlayerPrivate& player, MediaSourcePrivateClient& client)
    : MediaSourcePrivate(client)
    , m_player(player)
#if !RELEASE_LOG_DISABLED
    , m_logger(player.mediaPlayerLogger())
    , m_logIdentifier(player.mediaPlayerLogIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);
}

MediaSourcePrivateHaiku::~MediaSourcePrivateHaiku()
{
    ALWAYS_LOG(LOGIDENTIFIER);
}

MediaSourcePrivate::AddStatus MediaSourcePrivateHaiku::addSourceBuffer(const ContentType& contentType, RefPtr<SourceBufferPrivate>& outPrivate)
{
    ALWAYS_LOG(LOGIDENTIFIER, contentType);

    // FIXME: Check if contentType is supported
    // For now, always accept supported types
    outPrivate = SourceBufferPrivateHaiku::create(*this, contentType);
    return AddStatus::Ok;
}

void MediaSourcePrivateHaiku::durationChanged(const MediaTime& duration)
{
    ALWAYS_LOG(LOGIDENTIFIER, duration);
    // Notify player of duration change if needed
    // m_player.durationChanged();
}

void MediaSourcePrivateHaiku::markEndOfStream(EndOfStreamStatus status)
{
    ALWAYS_LOG(LOGIDENTIFIER, status);
    m_isEnded = true;
}

void MediaSourcePrivateHaiku::unmarkEndOfStream()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_isEnded = false;
}

bool MediaSourcePrivateHaiku::isEnded() const
{
    return m_isEnded;
}

MediaPlayer::ReadyState MediaSourcePrivateHaiku::readyState() const
{
    return m_readyState;
}

void MediaSourcePrivateHaiku::setReadyState(MediaPlayer::ReadyState readyState)
{
    ALWAYS_LOG(LOGIDENTIFIER, readyState);
    m_readyState = readyState;
}

void MediaSourcePrivateHaiku::waitForSeekCompleted(float, const MediaTime&, Promise&& promise)
{
    // FIXME: Implement
    promise.resolve();
}

void MediaSourcePrivateHaiku::seekToTime(const MediaTime& time)
{
    ALWAYS_LOG(LOGIDENTIFIER, time);
    // Delegate to SourceBuffers to seek
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaSourcePrivateHaiku::logChannel() const
{
    return LogMediaSource;
}
#endif

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)
