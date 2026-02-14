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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebsiteDataStore.h"
#include "WebsiteDataStoreParameters.h"

#include <WebCore/NotImplemented.h>
#include <FindDirectory.h>
#include <Path.h>

namespace WebKit {

void WebsiteDataStore::platformInitialize()
{
}

void WebsiteDataStore::platformDestroy()
{
}

#if !USE(CURL)
void WebsiteDataStore::platformSetNetworkParameters(WebsiteDataStoreParameters&)
{
}
#endif

String WebsiteDataStore::cacheDirectoryFileSystemRepresentation(const String& directoryName,
    const WTF::String& baseCacheDirectory, WebsiteDataStore::ShouldCreateDirectory)
{
    if (!baseCacheDirectory.isNull())
        return FileSystem::pathByAppendingComponent(baseCacheDirectory, directoryName);

    BPath path;
    if (find_directory(B_USER_CACHE_DIRECTORY, &path) == B_OK) {
        path.Append("WebKit");
        return FileSystem::pathByAppendingComponent(String::fromUTF8(path.Path()), directoryName);
    }
    return FileSystem::pathByAppendingComponent("/boot/home/config/cache/WebKit"_s, directoryName);
}

String WebsiteDataStore::websiteDataDirectoryFileSystemRepresentation(const String& directoryName,
    const WTF::String& baseDataDirectory, WebsiteDataStore::ShouldCreateDirectory)
{
    if (!baseDataDirectory.isNull())
        return FileSystem::pathByAppendingComponent(baseDataDirectory, directoryName);

    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append("WebKit/WebsiteData");
        return FileSystem::pathByAppendingComponent(String::fromUTF8(path.Path()), directoryName);
    }
    return FileSystem::pathByAppendingComponent("/boot/home/config/settings/WebKit/WebsiteData"_s, directoryName);
}

UnifiedOriginStorageLevel WebsiteDataStore::defaultUnifiedOriginStorageLevel()
{
    return UnifiedOriginStorageLevel::None;
}

} // namespace WebKit
