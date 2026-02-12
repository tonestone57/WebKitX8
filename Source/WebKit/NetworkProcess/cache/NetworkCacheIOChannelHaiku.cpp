/*
 * Copyright (C) 2019 Haiku Inc.,
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
#include "NetworkCacheIOChannel.h"

#include <WebCore/NotImplemented.h>

namespace WebKit {
namespace NetworkCache {

IOChannel::IOChannel(const String& filePath, Type type, std::optional<WorkQueue::QOS>)
{
    FileSystem::FileOpenMode openMode = FileSystem::FileOpenMode::Read;

    switch (type) {
    case Type::Read:
        openMode = FileSystem::FileOpenMode::Read;
        break;
    case Type::Write:
        openMode = FileSystem::FileOpenMode::ReadWrite;
        break;
    case Type::Create:
        openMode = FileSystem::FileOpenMode::ReadWrite;
        // Ensure file is created/truncated?
        // FileSystem::openFile with ReadWrite | Create usually works.
        // But explicitly deleting might be safer if we want truncation behavior for "Create".
        FileSystem::deleteFile(filePath);
        break;
    }

    m_fileDescriptor = FileSystem::openFile(filePath, openMode);
}

IOChannel::~IOChannel()
{
    if (FileSystem::isHandleValid(m_fileDescriptor))
        FileSystem::closeFile(m_fileDescriptor);
}

void IOChannel::read(size_t offset, size_t size, Ref<WTF::WorkQueueBase>&& queue, Function<void(Data&&, int error)>&& completionHandler)
{
    queue->dispatch([this, protectedThis = Ref { *this }, offset, size, completionHandler = WTFMove(completionHandler)]() mutable {
        Locker locker { m_lock };
        if (!FileSystem::isHandleValid(m_fileDescriptor)) {
            completionHandler(Data(), -1);
            return;
        }

        long long current = FileSystem::seekFile(m_fileDescriptor, offset, FileSystem::FileSeekOrigin::Beginning);
        if (current != (long long)offset) {
             completionHandler(Data(), -1);
             return;
        }

        Vector<uint8_t> buffer;
        buffer.resize(size);
        int bytesRead = FileSystem::readFromFile(m_fileDescriptor, buffer.data(), size);
        if (bytesRead < 0) {
            completionHandler(Data(), -1);
            return;
        }
        buffer.shrink(bytesRead);

        completionHandler(Data(WTFMove(buffer)), 0);
    });
}

void IOChannel::write(size_t offset, const Data& data, Ref<WTF::WorkQueueBase>&& queue, Function<void(int error)>&& completionHandler)
{
    // Copy data for async write
    Vector<uint8_t> buffer;
    auto span = data.span();
    buffer.append(span);

    queue->dispatch([this, protectedThis = Ref { *this }, offset, buffer = WTFMove(buffer), completionHandler = WTFMove(completionHandler)]() mutable {
         Locker locker { m_lock };
         if (!FileSystem::isHandleValid(m_fileDescriptor)) {
             completionHandler(-1);
             return;
         }

         long long current = FileSystem::seekFile(m_fileDescriptor, offset, FileSystem::FileSeekOrigin::Beginning);
         if (current != (long long)offset) {
             completionHandler(-1);
             return;
         }

         int bytesWritten = FileSystem::writeToFile(m_fileDescriptor, buffer.data(), buffer.size());
         if (bytesWritten < 0 || (size_t)bytesWritten != buffer.size()) {
             completionHandler(-1);
             return;
         }

         completionHandler(0);
    });
}

} // namespace NetworkCache
} // namespace WebKit
