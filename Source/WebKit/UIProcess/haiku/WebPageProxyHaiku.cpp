/*
 * Copyright (C) 2014-2021 Haiku, inc.
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
#include "WebKitVersion.h"
#include "WebPageProxy.h"

#include <WebCore/UserAgent.h>

#include <sys/utsname.h>

#include <Directory.h>
#include <DataIO.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <stdlib.h>

namespace WebKit {

void WebPageProxy::platformInitialize()
{
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append("WebKit");
        create_directory(path.Path(), 0755);
    }
}

String WebPageProxy::userAgentForURL(const URL&)
{
    return userAgent();
}

String WebPageProxy::standardUserAgent(const String& applicationNameForUserAgent)
{
    return WebCore::standardUserAgent(applicationNameForUserAgent);
}

static bool readMessageFromFile(int fd, BMessage& message)
{
    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size == 0)
        return false;

    char* buffer = (char*)malloc(st.st_size);
    if (!buffer) return false;

    ssize_t bytesRead = 0;
    while (bytesRead < st.st_size) {
        ssize_t r = read(fd, buffer + bytesRead, st.st_size - bytesRead);
        if (r < 0) {
            if (errno == EINTR) continue;
            free(buffer);
            return false;
        }
        if (r == 0) break;
        bytesRead += r;
    }

    if (bytesRead < st.st_size) {
        free(buffer);
        return false;
    }

    BMemoryIO io(buffer, st.st_size);
    status_t result = message.Unflatten(&io);
    free(buffer);
    return result == B_OK;
}

static bool writeMessageToFile(int fd, const BMessage& message)
{
    BMallocIO io;
    if (message.Flatten(&io) != B_OK)
        return false;

    // Use ftruncate after lock to ensure atomicity
    if (ftruncate(fd, 0) != 0 || lseek(fd, 0, SEEK_SET) != 0)
        return false;

    const char* buffer = (const char*)io.Buffer();
    size_t size = io.BufferLength();
    ssize_t bytesWritten = 0;
    while (bytesWritten < (ssize_t)size) {
        ssize_t w = write(fd, buffer + bytesWritten, size - bytesWritten);
        if (w < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        bytesWritten += w;
    }
    return true;
}

void WebPageProxy::saveRecentSearches(IPC::Connection&, const String& name, const Vector<WebCore::RecentSearch>& searchItems)
{
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
        return;
    path.Append("WebKit");
    create_directory(path.Path(), 0755);
    path.Append("RecentSearches");

    // Open without O_TRUNC to allow safe locking
    int fd = open(path.Path(), O_RDWR | O_CREAT, 0644);
    if (fd < 0) return;

    if (flock(fd, LOCK_EX) != 0) {
        close(fd);
        return;
    }

    BMessage message;
    readMessageFromFile(fd, message);

    BMessage searches;
    for (const auto& item : searchItems) {
        searches.AddString("items", item.string.utf8().data());
        searches.AddDouble("times", item.time.secondsSinceEpoch().seconds());
    }

    message.RemoveName(name.utf8().data());
    message.AddMessage(name.utf8().data(), &searches);

    writeMessageToFile(fd, message);

    flock(fd, LOCK_UN);
    close(fd);
}

void WebPageProxy::loadRecentSearches(IPC::Connection&, const String& name, CompletionHandler<void(Vector<WebCore::RecentSearch>&&)>&& completionHandler)
{
    Vector<WebCore::RecentSearch> items;
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append("WebKit/RecentSearches");

        int fd = open(path.Path(), O_RDONLY);
        if (fd >= 0) {
            if (flock(fd, LOCK_SH) == 0) {
                BMessage message;
                if (readMessageFromFile(fd, message)) {
                    BMessage searches;
                    if (message.FindMessage(name.utf8().data(), &searches) == B_OK) {
                        const char* item;
                        for (int32 i = 0; searches.FindString("items", i, &item) == B_OK; i++) {
                             WebCore::RecentSearch search;
                             search.string = String::fromUTF8(item);
                             double time;
                             if (searches.FindDouble("times", i, &time) == B_OK)
                                 search.time = WebCore::WallTime::fromRawSeconds(time);
                             items.append(search);
                        }
                    }
                }
                flock(fd, LOCK_UN);
            }
            close(fd);
        }
    }
    completionHandler(WTFMove(items));
}

void WebPageProxy::didUpdateEditorState(const EditorState&, const EditorState&)
{
}

}
