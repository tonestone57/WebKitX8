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
#include <WebCore/NotImplemented.h>
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

static const char* kSettingsPath = "WebKit/CertificateExceptions";

static void saveAllowedHosts()
{
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
        return;
    path.Append(kSettingsPath);

    BPath parent;
    path.GetParent(&parent);
    create_directory(parent.Path(), 0755);

    BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
    if (file.InitCheck() != B_OK)
        return;

    BMessage msg;
    {
        Locker locker { s_allowedHostsLock };
        for (const auto& host : allowedHosts()) {
            msg.AddString("host", host.utf8().data());
        }
    }
    msg.Flatten(&file);
}

static void loadAllowedHosts()
{
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
        return;
    path.Append(kSettingsPath);

    BFile file(path.Path(), B_READ_ONLY);
    if (file.InitCheck() != B_OK)
        return;

    BMessage msg;
    if (msg.Unflatten(&file) != B_OK)
        return;

    const char* host;
    for (int32 i = 0; msg.FindString("host", i, &host) == B_OK; i++) {
        Locker locker { s_allowedHostsLock };
        allowedHosts().add(String::fromUTF8(host));
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
    loadAllowedHosts();
}

void NetworkProcess::allowSpecificHTTPSCertificateForHost(const CertificateInfo& certificateInfo, const String& host)
{
    // Store the host in our local set to bypass verification in NetworkDataTaskHaiku.
    // This effectively persists the exception for this session and future sessions.
    addAllowedHTTPSCertificateHost(host);
    saveAllowedHosts();
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
            entry.Remove();
        } else if (entry.IsDirectory()) {
            BDirectory subDir(&entry);
            recursiveDelete(subDir, modifiedSince);
            entry.Remove();
        } else {
            time_t modificationTime;
            if (entry.GetModificationTime(&modificationTime) == B_OK) {
                if (WallTime::fromRawSeconds(modificationTime) >= modifiedSince) {
                    entry.Remove();
                }
            } else {
                entry.Remove();
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
            entry.Remove();
        }
    }
    completionHandler();
}

} // namespace WebKit
