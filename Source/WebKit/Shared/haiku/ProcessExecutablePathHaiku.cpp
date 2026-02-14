/*
 * Copyright (C) 2012 Samsung Electronics
 * Copyright (C) 2014,2019 Haiku, inc.
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
#include "ProcessExecutablePath.h"

#include <Entry.h>
#include <Path.h>
#include <Roster.h>
#include <String.h>
#include <image.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

static String getProcessPath(const char* name)
{
    // Try to find the executable relative to the current app/library location
    // This handles cases where we are running from a build directory or packaged app.

    // Get location of libWebKit.so
    // Note: The library name might be libWebKit.so, libWebKit.so.1, etc.
    int32 cookie = 0;
    image_info info;
    while (get_next_image_info(B_CURRENT_TEAM, &cookie, &info) == B_OK) {
        if (strstr(info.name, "libWebKit")) {
            BPath path(info.name);
            path.GetParent(&path);
            path.Append(name);
            BEntry entry(path.Path());
            if (entry.Exists())
                return String::fromUTF8(path.Path());
        }
    }

    // Fallback: Check if it's in the same directory as the app executable
    app_info appInfo;
    if (be_app && be_app->GetAppInfo(&appInfo) == B_OK) {
        BPath path(&appInfo.ref);
        path.GetParent(&path);
        path.Append(name);
        BEntry entry(path.Path());
        if (entry.Exists())
            return String::fromUTF8(path.Path());
    }

    // Fallback: Check environment variable WEBKIT_EXEC_PATH
    const char* envPath = getenv("WEBKIT_EXEC_PATH");
    if (envPath) {
        BPath path(envPath);
        path.Append(name);
        BEntry entry(path.Path());
        if (entry.Exists())
            return String::fromUTF8(path.Path());
    }

    // Fallback: Check standard install locations
    // WebKit add-ons usually install helpers in /boot/system/lib/WebKit or /boot/home/config/non-packaged/lib/WebKit
    const char* searchPaths[] = {
        "/boot/system/lib/WebKit",
        "/boot/home/config/non-packaged/lib/WebKit",
        "/boot/system/non-packaged/lib/WebKit"
    };

    for (const char* searchPath : searchPaths) {
        BPath path(searchPath);
        path.Append(name);
        BEntry entry(path.Path());
        if (entry.Exists())
            return String::fromUTF8(path.Path());
    }

    return String::fromUTF8(name); // Hope it's in PATH?
}

String executablePathOfWebProcess()
{
    static NeverDestroyed<String> path = getProcessPath("WebProcess");
    return path;
}

String executablePathOfPluginProcess()
{
    static NeverDestroyed<String> path = getProcessPath("PluginProcess");
    return path;
}

String executablePathOfNetworkProcess()
{
    static NeverDestroyed<String> path = getProcessPath("NetworkProcess");
    return path;
}

} // namespace WebKit

