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

#pragma once

#if ENABLE(MEDIA_SOURCE)

#include "SourceBufferPrivate.h"
#include <wtf/LoggerHelper.h>

namespace WebCore {

class SourceBufferPrivateHaiku final : public SourceBufferPrivate
#if !RELEASE_LOG_DISABLED
    , public LoggerHelper
#endif
{
public:
    static Ref<SourceBufferPrivateHaiku> create(MediaSourcePrivate&);

    SourceBufferPrivateHaiku(MediaSourcePrivate&);
    virtual ~SourceBufferPrivateHaiku();

    constexpr MediaPlatformType platformType() const override { return MediaPlatformType::Haiku; }

    Ref<MediaPromise> appendInternal(Ref<SharedBuffer>&&) override;
    void resetParserStateInternal() override;
    void removedFromMediaSource() override;
    bool isReadyForMoreSamples(TrackID) override { return true; }
    void notifyClientWhenReadyForMoreSamples(TrackID) override;
    bool canSetMinimumUpcomingPresentationTime(TrackID) const override { return false; }
    void setMinimumUpcomingPresentationTime(TrackID, const MediaTime&) override { }

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger; }
    const char* logClassName() const final { return "SourceBufferPrivateHaiku"; }
    const void* logIdentifier() const final { return reinterpret_cast<const void*>(m_logIdentifier); }
    WTFLogChannel& logChannel() const final;

    const Logger& sourceBufferLogger() const final { return m_logger; }
    uint64_t sourceBufferLogIdentifier() final { return m_logIdentifier; }
#endif

private:
#if !RELEASE_LOG_DISABLED
    Ref<Logger> m_logger;
    uint64_t m_logIdentifier;
#endif
};

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)
