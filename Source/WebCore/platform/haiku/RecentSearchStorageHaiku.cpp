/*
 * Copyright (C) 2024 Haiku, inc.
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
#include "RecentSearchStorageHaiku.h"

#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>
#include <wtf/text/CString.h>

namespace WebCore {

namespace RecentSearchStorage {

void save(const String& name, const Vector<RecentSearch>& searchItems)
{
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
        return;
    path.Append("WebKit");
    create_directory(path.Path(), 0755);
    path.Append("RecentSearches");

    BMessage message;
    {
        BFile file(path.Path(), B_READ_ONLY);
        if (file.InitCheck() == B_OK)
            message.Unflatten(&file);
    }

    BMessage searches;
    for (const auto& item : searchItems) {
        searches.AddString("items", item.string.utf8().data());
        searches.AddDouble("times", item.time.secondsSinceEpoch().seconds());
    }

    message.RemoveName(name.utf8().data());
    message.AddMessage(name.utf8().data(), &searches);

    BPath tempPath(path);
    if (tempPath.GetParent(&tempPath) != B_OK)
        return;
    tempPath.Append("RecentSearches.tmp");

    BFile tempFile(tempPath.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
    if (tempFile.InitCheck() == B_OK) {
        if (message.Flatten(&tempFile) == B_OK) {
            tempFile.Unset();
            BEntry tempEntry(tempPath.Path());
            tempEntry.Rename(path.Leaf(), true);
        }
    }
}

Vector<RecentSearch> load(const String& name)
{
    Vector<RecentSearch> items;
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append("WebKit/RecentSearches");
        BFile file(path.Path(), B_READ_ONLY);
        BMessage message;
        if (file.InitCheck() == B_OK && message.Unflatten(&file) == B_OK) {
            BMessage searches;
            if (message.FindMessage(name.utf8().data(), &searches) == B_OK) {
                const char* item;
                for (int32 i = 0; searches.FindString("items", i, &item) == B_OK; i++) {
                    RecentSearch search;
                    search.string = String::fromUTF8(item);
                    double time;
                    if (searches.FindDouble("times", i, &time) == B_OK)
                        search.time = WallTime::fromRawSeconds(time);
                    items.append(search);
                }
            }
        }
    }
    return items;
}

} // namespace RecentSearchStorage

} // namespace WebCore
