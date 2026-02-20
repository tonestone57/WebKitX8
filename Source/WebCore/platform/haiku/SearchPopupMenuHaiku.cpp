/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "SearchPopupMenuHaiku.h"


#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>

namespace WebCore {

SearchPopupMenuHaiku::SearchPopupMenuHaiku(PopupMenuClient* client)
    : m_popup(adoptRef(new PopupMenuHaiku(client)))
{
}

void SearchPopupMenuHaiku::saveRecentSearches(const AtomString& name, const Vector<RecentSearch>& searchItems)
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

    message.RemoveName(name.string().utf8().data());
    message.AddMessage(name.string().utf8().data(), &searches);

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

void SearchPopupMenuHaiku::loadRecentSearches(const AtomString& name, Vector<RecentSearch>& searchItems)
{
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append("WebKit/RecentSearches");
        BFile file(path.Path(), B_READ_ONLY);
        BMessage message;
        if (file.InitCheck() == B_OK && message.Unflatten(&file) == B_OK) {
            BMessage searches;
            if (message.FindMessage(name.string().utf8().data(), &searches) == B_OK) {
                const char* item;
                for (int32 i = 0; searches.FindString("items", i, &item) == B_OK; i++) {
                    RecentSearch search;
                    search.string = String::fromUTF8(item);
                    double time;
                    if (searches.FindDouble("times", i, &time) == B_OK)
                        search.time = WallTime::fromRawSeconds(time);
                    searchItems.append(search);
                }
            }
        }
    }
}

bool SearchPopupMenuHaiku::enabled()
{
    return true;
}

PopupMenu* SearchPopupMenuHaiku::popupMenu()
{
    return m_popup.get();
}

} // namespace WebCore
