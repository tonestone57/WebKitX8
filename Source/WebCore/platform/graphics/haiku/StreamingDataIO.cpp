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
#include "StreamingDataIO.h"

#if ENABLE(MEDIA_SOURCE)

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace WebCore {

// StreamingDataController

Ref<StreamingDataController> StreamingDataController::create()
{
    return adoptRef(*new StreamingDataController());
}

StreamingDataController::StreamingDataController()
    : m_fd(-1)
    , m_writePosition(0)
    , m_eos(false)
{
    char path[] = "/tmp/WebKit-StreamingData-XXXXXX";
    m_fd = mkstemp(path);
    if (m_fd >= 0)
        unlink(path);
}

StreamingDataController::~StreamingDataController()
{
    if (m_fd >= 0)
        close(m_fd);
}

bool StreamingDataController::append(const void* data, size_t size)
{
    Locker locker { m_lock };
    if (m_fd < 0)
        return false;

    ssize_t written = pwrite(m_fd, data, size, m_writePosition);
    if (written < 0 || (size_t)written != size)
        return false;

    m_writePosition += size;
    m_condition.notifyAll();
    return true;
}

ssize_t StreamingDataController::read(off_t position, void* buffer, size_t size)
{
    Locker locker { m_lock };

    while (position >= m_writePosition && !m_eos) {
        m_condition.wait(m_lock);
    }

    if (position >= m_writePosition)
        return 0; // EOF

    if (m_fd < 0)
        return B_ERROR;

    return pread(m_fd, buffer, size, position);
}

off_t StreamingDataController::getSize() const
{
    Locker locker { m_lock };
    return m_writePosition;
}

void StreamingDataController::setEOS()
{
    Locker locker { m_lock };
    m_eos = true;
    m_condition.notifyAll();
}

void StreamingDataController::clearEOS()
{
    Locker locker { m_lock };
    m_eos = false;
}

bool StreamingDataController::isEOS() const
{
    Locker locker { m_lock };
    return m_eos;
}

// StreamingDataIO

StreamingDataIO::StreamingDataIO(Ref<StreamingDataController>&& controller)
    : m_controller(WTFMove(controller))
    , m_position(0)
{
}

StreamingDataIO::~StreamingDataIO()
{
}

ssize_t StreamingDataIO::Read(void* buffer, size_t size)
{
    ssize_t bytesRead = m_controller->read(m_position, buffer, size);
    if (bytesRead > 0)
        m_position += bytesRead;
    return bytesRead;
}

ssize_t StreamingDataIO::ReadAt(off_t position, void* buffer, size_t size)
{
    return m_controller->read(position, buffer, size);
}

ssize_t StreamingDataIO::Write(const void* buffer, size_t size)
{
    // Write is not supported for reading side (BMediaFile reads)
    // But if BMediaFile tries to write? Unlikely for playback.
    return B_NOT_SUPPORTED;
}

ssize_t StreamingDataIO::WriteAt(off_t position, const void* buffer, size_t size)
{
    return B_NOT_SUPPORTED;
}

off_t StreamingDataIO::Seek(off_t position, uint32 seekMode)
{
    switch (seekMode) {
    case SEEK_SET:
        m_position = position;
        break;
    case SEEK_CUR:
        m_position += position;
        break;
    case SEEK_END:
        m_position = m_controller->getSize() + position;
        break;
    }
    return m_position;
}

off_t StreamingDataIO::Position() const
{
    return m_position;
}

status_t StreamingDataIO::SetSize(off_t size)
{
    return B_NOT_SUPPORTED;
}

status_t StreamingDataIO::GetSize(off_t* size) const
{
    *size = m_controller->getSize();
    return B_OK;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)
