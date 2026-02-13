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

#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>
#include <fs_attr.h>
#include <unistd.h>


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

bool fileExists(const String& path)
{
    BEntry entry(path.utf8().data());
    return entry.Exists();
}

bool deleteNonEmptyDirectory(const String& path)
{
    // BEntry::Remove() handles recursive deletion for directories?
    // "If the entry is a directory, it must be empty to be removed." - BeBook
    // So we need to implement recursive deletion.

    BDirectory dir(path.utf8().data());
    if (dir.InitCheck() != B_OK)
        return false;

    BEntry entry;
    while (dir.GetNextEntry(&entry) == B_OK) {
        if (entry.IsDirectory()) {
            BPath subPath;
            entry.GetPath(&subPath);
            if (!deleteNonEmptyDirectory(String::fromUTF8(subPath.Path())))
                return false;
        } else {
            if (entry.Remove() != B_OK)
                return false;
        }
    }
    return dir.GetEntry(&entry) == B_OK && entry.Remove() == B_OK;
}

String pathGetFileName(const String& path)
{
    BPath bpath(path.utf8().data());
    return String::fromUTF8(bpath.Leaf());
}

String directoryName(const String& path)
{
    BPath bpath(path.utf8().data());
    BPath parent;
    if (bpath.GetParent(&parent) == B_OK)
        return String::fromUTF8(parent.Path());
    return String();
}

bool makeAllDirectories(const String& path)
{
    return create_directory(path.utf8().data(), 0777) == B_OK;
}

PlatformFileHandle openFile(const String& path, FileOpenMode mode)
{
    int flags = 0;
    switch (mode) {
    case FileOpenMode::Read:
        flags = B_READ_ONLY;
        break;
    case FileOpenMode::Write:
        flags = B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE;
        break;
    case FileOpenMode::ReadWrite:
        flags = B_READ_WRITE | B_CREATE_FILE;
        break;
#if ENABLE(FILE_SYSTEM_POSIX)
    case FileOpenMode::Append:
        flags = B_WRITE_ONLY | B_CREATE_FILE | B_OPEN_AT_END;
        break;
#endif
    }

    BFile* file = new BFile(path.utf8().data(), flags);
    if (file->InitCheck() != B_OK) {
        delete file;
        return invalidPlatformFileHandle;
    }
    return file;
}

void closeFile(PlatformFileHandle& handle)
{
    if (handle != invalidPlatformFileHandle) {
        delete handle;
        handle = invalidPlatformFileHandle;
    }
}

long long seekFile(PlatformFileHandle handle, long long offset, FileSeekOrigin origin)
{
    if (handle == invalidPlatformFileHandle)
        return -1;

    int seekMode = SEEK_SET;
    switch (origin) {
    case FileSeekOrigin::Beginning:
        seekMode = SEEK_SET;
        break;
    case FileSeekOrigin::Current:
        seekMode = SEEK_CUR;
        break;
    case FileSeekOrigin::End:
        seekMode = SEEK_END;
        break;
    }

    return handle->Seek(offset, seekMode);
}

int writeToFile(PlatformFileHandle handle, const void* data, int length)
{
    if (handle == invalidPlatformFileHandle)
        return -1;
    return handle->Write(data, length);
}

int readFromFile(PlatformFileHandle handle, void* data, int length)
{
    if (handle == invalidPlatformFileHandle)
        return -1;
    return handle->Read(data, length);
}

bool fileIsDirectory(const String& path)
{
    BEntry entry(path.utf8().data());
    return entry.IsDirectory();
}

bool hardLink(const String& targetPath, const String& linkPath)
{
    return link(targetPath.utf8().data(), linkPath.utf8().data()) == 0;
}

bool symlink(const String& targetPath, const String& linkPath)
{
    return ::symlink(targetPath.utf8().data(), linkPath.utf8().data()) == 0;
}

std::optional<int32_t> getVolumeId(const String& path)
{
    BEntry entry(path.utf8().data());
    entry_ref ref;
    if (entry.GetRef(&ref) == B_OK)
        return ref.device;
    return std::nullopt;
}

} // namespace FileSystem
} // namespace WebCore
