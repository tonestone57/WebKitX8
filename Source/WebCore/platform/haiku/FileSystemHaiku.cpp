/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 *
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "wtf/FileSystem.h"

#include <wtf/text/CString.h>

#include <Entry.h>
#include <FindDirectory.h>
#include <Path.h>


namespace WebCore {

namespace FileSystem {

CString fileSystemRepresentation(const String& path)
{
    return path.utf8();
}

String stringFromFileSystemRepresentation(const char* fileSystemRepresentation)
{
    return String::fromUTF8(fileSystemRepresentation);
}

String homeDirectoryPath()
{
    BPath path;
    if (find_directory(B_USER_DIRECTORY, &path) != B_OK)
        return String();

    return String::fromUTF8(path.Path());
}

bool deleteFile(const String& path)
{
    BEntry entry(path.utf8().data());
    return entry.Remove() == B_OK;
}

bool deleteEmptyDirectory(const String& path)
{
    BEntry entry(path.utf8().data());
    return entry.Remove() == B_OK;
}

long long fileSize(const String& path)
{
    BEntry entry(path.utf8().data());
    off_t size;
    if (entry.GetSize(&size) == B_OK)
        return size;
    return -1;
}

std::optional<WallTime> fileModificationTime(const String& path)
{
    BEntry entry(path.utf8().data());
    time_t time;
    if (entry.GetModificationTime(&time) == B_OK)
        return WallTime::fromRawSeconds(time);
    return std::nullopt;
}

} // namespace FileSystem
} // namespace WebCore
