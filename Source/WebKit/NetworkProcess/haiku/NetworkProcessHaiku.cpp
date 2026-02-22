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
#include "NetworkProcess.h"
#include "NetworkProcessHaiku.h"

#include "NetworkProcessCreationParameters.h"
#include "CertificateUtilitiesHaiku.h"
#include <WebCore/NotImplemented.h>
#include <wtf/Assertions.h>
#include <wtf/Language.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <stdio.h>

#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>
#include <Message.h>

namespace WebKit {

using namespace WebCore;

static Lock s_allowedHostsLock;
static HashSet<String>& allowedHosts()
{
    static NeverDestroyed<HashSet<String>> hosts;
    return hosts;
}

// Helper to populate cache from file (simplistic reading only)
// Note: This duplicates reading logic but avoids parsing fingerprints for simple host checks.
// Actually, isHTTPSCertificateAllowed(host) scans the file. To cache it, we'd need to parse the file.
// Since the file format is custom (host fingerprint\n), we can parse it.
// However, synchronizing the cache with external changes is hard.
// Given this process is the *only* one likely adding exceptions (via UI interaction),
// we can cache additions locally.
// But initial load?
// We will populate on first use or just cache *added* hosts.
// To avoid hitting disk on *every* request for a host that *isn't* allowed, we can't easily cache negative results without monitoring.
// But for positive results (allowed hosts), we can cache them.
// Actually, the previous implementation loaded the list on startup.
// Let's do that: load once on startup using the shared utility if possible, or manual parse.
// Since we want to use the shared utility's format, we should probably add a function there to "getAllAllowedHosts".
// But `CertificateUtilitiesHaiku` is in Shared/ so we can use it.
// For now, let's just cache the hosts we *add* in this session, and rely on disk for others?
// No, that's inconsistent.
// Better: Keep the cache. Populate it on startup by reading the file.
// When adding, update cache and file.
// When checking, check cache.
// Limitation: If another process adds an exception, we won't see it until restart.
// This is acceptable and matches previous behavior (and most browsers).

static void populateAllowedHosts()
{
    // We need to read the file. Since CertificateUtilitiesHaiku hides the path/format,
    // we should ideally expose a "getAllHosts" or just re-implement reading here matching the format.
    // The format is "host fingerprint" or "host".
    // We can reuse the path logic.
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
        return;
    path.Append("WebKit/certificate_exceptions");

    BFile file(path.Path(), B_READ_ONLY);
    if (file.InitCheck() != B_OK)
        return;

    off_t size;
    file.GetSize(&size);
    if (size <= 0) return;

    BString content;
    char* buffer = content.LockBuffer(size);
    if (file.Read(buffer, size) != size) {
        content.UnlockBuffer(0);
        return;
    }
    content.UnlockBuffer(size);

    int32 start = 0;
    int32 end;
    Locker locker { s_allowedHostsLock };
    while ((end = content.FindFirst('\n', start)) != B_ERROR) {
        BString line;
        content.CopyInto(line, start, end - start);
        start = end + 1;
        if (line.IsEmpty()) continue;

        int32 spacePos = line.FindFirst(' ');
        if (spacePos != B_ERROR) {
            BString host;
            line.CopyInto(host, 0, spacePos);
            allowedHosts().add(String::fromUTF8(host.String()));
        } else {
            allowedHosts().add(String::fromUTF8(line.String()));
        }
    }
    // Handle last line
    if (start < content.Length()) {
        BString line;
        content.CopyInto(line, start, content.Length() - start);
        if (!line.IsEmpty()) {
            int32 spacePos = line.FindFirst(' ');
            if (spacePos != B_ERROR) {
                BString host;
                line.CopyInto(host, 0, spacePos);
                allowedHosts().add(String::fromUTF8(host.String()));
            } else {
                allowedHosts().add(String::fromUTF8(line.String()));
            }
        }
    }
}

void addAllowedHTTPSCertificateHost(const String& host)
{
    Locker locker { s_allowedHostsLock };
    allowedHosts().add(host);
}

bool isHTTPSCertificateHostAllowed(const String& host)
{
    Locker locker { s_allowedHostsLock };
    return allowedHosts().contains(host);
}

void NetworkProcess::platformInitializeNetworkProcess(const NetworkProcessCreationParameters& parameters)
{
    WTF::listenForLanguageChangeNotifications();
    populateAllowedHosts();
}

void NetworkProcess::allowSpecificHTTPSCertificateForHost(const CertificateInfo& certificateInfo, const String& host)
{
    addHTTPSCertificateException(host, certificateInfo);
    // Update local cache
    addAllowedHTTPSCertificateHost(host);
}

void NetworkProcess::platformTerminate()
{
}

static void recursiveDelete(BDirectory& dir, WallTime modifiedSince)
{
    BEntry entry;
    dir.Rewind();
    while (dir.GetNextEntry(&entry) == B_OK) {
        if (entry.IsSymLink()) {
            time_t modificationTime;
            if (entry.GetModificationTime(&modificationTime) == B_OK) {
                if (WallTime::fromRawSeconds(modificationTime) >= modifiedSince) {
                    if (entry.Remove() != B_OK)
                        WTFLogAlways("Failed to remove symlink\n");
                }
            } else {
                if (entry.Remove() != B_OK)
                    WTFLogAlways("Failed to remove symlink\n");
            }
        } else if (entry.IsDirectory()) {
            BDirectory subDir(&entry);
            recursiveDelete(subDir, modifiedSince);
            // If the directory still contains files (because they were preserved), Remove() will fail with B_DIRECTORY_NOT_EMPTY.
            // We ignore that error intentionally to preserve older content.
            status_t result = entry.Remove();
            if (result != B_OK && result != B_DIRECTORY_NOT_EMPTY)
                WTFLogAlways("Failed to remove directory: %s\n", strerror(result));
        } else {
            time_t modificationTime;
            if (entry.GetModificationTime(&modificationTime) == B_OK) {
                if (WallTime::fromRawSeconds(modificationTime) >= modifiedSince) {
                    if (entry.Remove() != B_OK)
                        WTFLogAlways("Failed to remove file\n");
                }
            } else {
                if (entry.Remove() != B_OK)
                    WTFLogAlways("Failed to remove file\n");
            }
        }
    }
}

void NetworkProcess::clearDiskCache(WallTime modifiedSince, CompletionHandler<void()>&& completionHandler)
{
    BPath path;
    if (find_directory(B_USER_CACHE_DIRECTORY, &path) == B_OK) {
        path.Append("WebKit");
        BEntry entry(path.Path());
        if (entry.Exists() && entry.IsDirectory()) {
            BDirectory dir(path.Path());
            recursiveDelete(dir, modifiedSince);
            // Try to remove the root WebKit cache dir if empty.
            entry.Remove();
        }
    }
    completionHandler();
}

} // namespace WebKit
