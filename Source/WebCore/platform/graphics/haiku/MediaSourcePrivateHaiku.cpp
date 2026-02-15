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
#include "MediaSourcePrivateHaiku.h"

#if ENABLE(MEDIA_SOURCE)

#include "MediaSourcePrivateClient.h"
#include "SourceBufferPrivateHaiku.h"
#include <wtf/Ref.h>

namespace WebCore {

Ref<MediaSourcePrivate> MediaSourcePrivate::create(MediaSourcePrivateClient& client)
{
    return adoptRef(*new MediaSourcePrivateHaiku(client));
}

Ref<MediaSourcePrivateHaiku> MediaSourcePrivateHaiku::create(MediaSourcePrivateClient& client)
{
    return adoptRef(*new MediaSourcePrivateHaiku(client));
}

MediaSourcePrivateHaiku::MediaSourcePrivateHaiku(MediaSourcePrivateClient& client)
    : MediaSourcePrivate(client)
{
}

MediaSourcePrivateHaiku::~MediaSourcePrivateHaiku()
{
}

MediaSourcePrivate::AddStatus MediaSourcePrivateHaiku::addSourceBuffer(const ContentType&, const MediaSourceConfiguration&, RefPtr<SourceBufferPrivate>& buffer)
{
    buffer = SourceBufferPrivateHaiku::create(*this);
    return MediaSourcePrivate::AddStatus::Ok;
}

void MediaSourcePrivateHaiku::notifyActiveSourceBuffersChanged()
{
}

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)
