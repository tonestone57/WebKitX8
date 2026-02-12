/*
 * Copyright (C) 2019 Haiku, Inc.
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
#include "NetworkCacheData.h"

#include <WebCore/SharedMemory.h>
#include <wtf/FileSystem.h>
#include <wtf/Variant.h>

namespace WebKit {
namespace NetworkCache {

Data::Data(std::span<const uint8_t> span)
{
    Vector<uint8_t> data;
    data.append(span);
    m_buffer = Box<Variant<Vector<uint8_t>, FileSystem::MappedFileData>>::create(WTFMove(data));
}

Data::Data(Vector<uint8_t>&& data)
    : m_buffer(Box<Variant<Vector<uint8_t>, FileSystem::MappedFileData>>::create(WTFMove(data)))
{
}

#if PLATFORM(HAIKU) && !USE(CURL)
Data::Data(Variant<Vector<uint8_t>, FileSystem::MappedFileData>&& data)
    : m_buffer(Box<Variant<Vector<uint8_t>, FileSystem::MappedFileData>>::create(WTFMove(data)))
{
}
#endif

Data Data::empty()
{
    return Data(Vector<uint8_t>());
}

std::span<const uint8_t> Data::span() const
{
    if (!m_buffer)
        return { };

    return WTF::switchOn(*m_buffer,
        [](const Vector<uint8_t>& vector) { return std::span<const uint8_t>(vector); },
        [](const FileSystem::MappedFileData& map) { return std::span<const uint8_t>(static_cast<const uint8_t*>(map.data()), map.size()); }
    );
}

size_t Data::size() const
{
    if (!m_buffer)
        return 0;

    return WTF::switchOn(*m_buffer,
        [](const Vector<uint8_t>& vector) { return vector.size(); },
        [](const FileSystem::MappedFileData& map) { return map.size(); }
    );
}

bool Data::isNull() const
{
    return !m_buffer;
}

bool Data::apply(const Function<bool(std::span<const uint8_t>)>& applier) const
{
    if (isNull())
        return applier({ });

    return applier(span());
}

Data Data::subrange(size_t offset, size_t size) const
{
    if (isNull() || offset >= this->size())
        return empty();

    size_t available = this->size() - offset;
    size = std::min(size, available);

    Vector<uint8_t> sub;
    sub.reserveInitialCapacity(size);
    sub.append(span().subspan(offset, size));

    return Data(WTFMove(sub));
}

Data concatenate(const Data& a, const Data& b)
{
    Vector<uint8_t> data;
    data.reserveInitialCapacity(a.size() + b.size());
    data.append(a.span());
    data.append(b.span());
    return Data(WTFMove(data));
}

Data Data::adoptMap(FileSystem::MappedFileData&& map, FileSystem::FileHandle&&)
{
    return Data(Variant<Vector<uint8_t>, FileSystem::MappedFileData>(WTFMove(map)));
}

RefPtr<WebCore::SharedMemory> Data::tryCreateSharedMemory() const
{
    return WebCore::SharedMemory::create(span());
}

Data mapFile(const String& path)
{
    bool success;
    FileSystem::MappedFileData map(path, FileSystem::MappedFileMode::Shared, success);
    if (!success)
        return Data();

    return Data::adoptMap(WTFMove(map), FileSystem::FileHandle());
}

Data Data::mapToFile(const String& path) const
{
    auto handle = FileSystem::openFile(path, FileSystem::FileOpenMode::ReadWrite);
    if (!FileSystem::isHandleValid(handle))
        return Data();

    FileSystem::writeToFile(handle, span());
    FileSystem::closeFile(handle);

    return mapFile(path);
}

bool bytesEqual(const Data& a, const Data& b)
{
    if (a.size() != b.size())
        return false;
    return memcmp(a.span().data(), b.span().data(), a.size()) == 0;
}

SHA1::Digest computeSHA1(const Data& data, const Salt& salt)
{
    SHA1 sha1;
    if (salt)
        sha1.addBytes(salt.value());
    sha1.addBytes(data.span());

    SHA1::Digest digest;
    sha1.computeHash(digest);
    return digest;
}

} // namespace NetworkCache
} // namespace WebKit
