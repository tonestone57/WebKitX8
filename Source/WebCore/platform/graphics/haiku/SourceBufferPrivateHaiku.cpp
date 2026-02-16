/*
 * Copyright (C) 2024 Haiku, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SourceBufferPrivateHaiku.h"

#if ENABLE(MEDIA_SOURCE)

#include "Logging.h"
#include "MediaSourcePrivateHaiku.h"
#include "SharedBuffer.h"
#include <wtf/Ref.h>

namespace WebCore {

Ref<SourceBufferPrivateHaiku> SourceBufferPrivateHaiku::create(MediaSourcePrivate& mediaSource)
{
    return adoptRef(*new SourceBufferPrivateHaiku(mediaSource));
}

SourceBufferPrivateHaiku::SourceBufferPrivateHaiku(MediaSourcePrivate& mediaSource)
    : SourceBufferPrivate(mediaSource)
    , m_streamingData(StreamingDataController::create())
#if !RELEASE_LOG_DISABLED
    , m_logger(Logger::create(this))
    , m_logIdentifier(LoggerHelper::uniqueLogIdentifier())
#endif
{
}

SourceBufferPrivateHaiku::~SourceBufferPrivateHaiku()
{
    if (m_streamingData)
        m_streamingData->setEOS();
}

Ref<MediaPromise> SourceBufferPrivateHaiku::appendInternal(Ref<SharedBuffer>&& buffer)
{
    if (m_streamingData) {
        for (const auto& segment : *buffer) {
            if (!m_streamingData->append(segment.data(), segment.size()))
                return MediaPromise::createAndReject(PlatformMediaError::IOError);
        }
    }
    return MediaPromise::createAndResolve();
}

void SourceBufferPrivateHaiku::resetParserStateInternal()
{
}

void SourceBufferPrivateHaiku::removedFromMediaSource()
{
    SourceBufferPrivate::removedFromMediaSource();
}

void SourceBufferPrivateHaiku::notifyClientWhenReadyForMoreSamples(TrackID)
{
}

void SourceBufferPrivateHaiku::setMediaSourceEnded(bool ended)
{
    SourceBufferPrivate::setMediaSourceEnded(ended);
    if (m_streamingData) {
        if (ended)
            m_streamingData->setEOS();
        else
            m_streamingData->clearEOS();
    }
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& SourceBufferPrivateHaiku::logChannel() const
{
    return LogMediaSource;
}
#endif

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)
