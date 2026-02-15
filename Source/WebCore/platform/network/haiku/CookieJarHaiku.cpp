/*
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
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

#include "CookieStorage.h"

#include "NetworkStorageSession.h"
#include "NotImplemented.h"

#include <functional>
#include <stdio.h>

#include <wtf/HashMap.h>
#include <wtf/text/CString.h>
#include <wtf/RunLoop.h>

#include <PathMonitor.h>
#include <Handler.h>
#include <Looper.h>
#include <Path.h>
#include <FindDirectory.h>

#define TRACE_COOKIE_JAR 0


namespace WebCore {

class CookieWatcher : public BHandler {
public:
    CookieWatcher() : BHandler("CookieWatcher") {}

    void MessageReceived(BMessage* message) override
    {
        if (message->what == B_PATH_MONITOR) {
            RunLoop::main().dispatch([this] {
                if (m_callback)
                    m_callback();
            });
        } else {
            BHandler::MessageReceived(message);
        }
    }

    void setCallback(WTF::Function<void()>&& callback)
    {
        m_callback = WTFMove(callback);
    }

private:
    WTF::Function<void()> m_callback;
};

static CookieWatcher* s_watcher = nullptr;
static BLooper* s_looper = nullptr;

void setCookieStoragePrivateBrowsingEnabled(bool)
{
}

void startObservingCookieChanges(NetworkStorageSession& storageSession, WTF::Function<void ()>&& callback)
{
    if (storageSession.sessionID().isEphemeral())
        return;

    if (!s_looper) {
        s_looper = new BLooper("CookieMonitorLooper");
        s_watcher = new CookieWatcher();
        s_looper->AddHandler(s_watcher);
        s_looper->Run();
    }

    s_watcher->setCallback(WTFMove(callback));

    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append("WebKit/Cookies");
        BPathMonitor::StartWatching(path.Path(), B_WATCH_STAT | B_WATCH_FILES_ONLY, s_watcher);
    }
}

void stopObservingCookieChanges(NetworkStorageSession& storageSession)
{
    if (storageSession.sessionID().isEphemeral())
        return;

    if (s_watcher) {
        BPathMonitor::StopWatching(s_watcher);
        s_watcher->setCallback(nullptr);
    }

    if (s_looper) {
        if (s_looper->Lock()) {
            if (s_watcher) {
                s_looper->RemoveHandler(s_watcher);
                delete s_watcher;
                s_watcher = nullptr;
            }
            s_looper->Quit();
            s_looper = nullptr;
        }
    }
}

} // namespace WebCore
