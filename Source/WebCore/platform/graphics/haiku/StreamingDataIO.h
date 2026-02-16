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

#include <DataIO.h>
#include <OS.h>
#include <wtf/Condition.h>
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class StreamingDataController : public ThreadSafeRefCounted<StreamingDataController> {
public:
    static Ref<StreamingDataController> create();

    StreamingDataController();
    ~StreamingDataController();

    void append(const void* data, size_t size);
    ssize_t read(off_t position, void* buffer, size_t size);
    off_t getSize() const;

    void setEOS();
    bool isEOS() const;

private:
    mutable Lock m_lock;
    Condition m_condition;
    Vector<uint8_t> m_buffer;
    bool m_eos;
};

class StreamingDataIO : public BPositionIO {
public:
    StreamingDataIO(Ref<StreamingDataController>&& controller);
    virtual ~StreamingDataIO();

    ssize_t Read(void* buffer, size_t size) override;
    ssize_t ReadAt(off_t position, void* buffer, size_t size) override;
    ssize_t Write(const void* buffer, size_t size) override;
    ssize_t WriteAt(off_t position, const void* buffer, size_t size) override;

    off_t Seek(off_t position, uint32 seekMode) override;
    off_t Position() const override;
    status_t SetSize(off_t size) override;
    status_t GetSize(off_t* size) const override;

private:
    Ref<StreamingDataController> m_controller;
    off_t m_position;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)
