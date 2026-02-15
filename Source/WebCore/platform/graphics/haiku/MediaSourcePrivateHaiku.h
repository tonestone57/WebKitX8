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

#include "MediaSourcePrivate.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class MediaSourcePrivateHaiku final : public MediaSourcePrivate {
public:
    static Ref<MediaSourcePrivateHaiku> create(MediaSourcePrivateClient&);

    MediaSourcePrivateHaiku(MediaSourcePrivateClient&);
    virtual ~MediaSourcePrivateHaiku();

    RefPtr<MediaPlayerPrivateInterface> player() const override { return m_player.get(); }
    void setPlayer(MediaPlayerPrivateInterface* player) override { m_player = player; }

    constexpr MediaPlatformType platformType() const override { return MediaPlatformType::Haiku; }

    AddStatus addSourceBuffer(const ContentType&, const MediaSourceConfiguration&, RefPtr<SourceBufferPrivate>&) override;
    void notifyActiveSourceBuffersChanged() override;

private:
    WeakPtr<MediaPlayerPrivateInterface> m_player;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)
