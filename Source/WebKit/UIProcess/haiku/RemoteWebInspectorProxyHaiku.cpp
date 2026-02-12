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
#include "RemoteWebInspectorUIProxy.h"

#include "APIPageConfiguration.h"
#include "WebViewBase.h"
#include "WebPageProxy.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/InspectorFrontendClient.h>

#include <Alert.h>
#include <Entry.h>
#include <File.h>
#include <Message.h>
#include <Rect.h>
#include <Roster.h>
#include <Window.h>

namespace WebKit {

class InspectorWindow : public BWindow {
public:
    InspectorWindow(BRect frame)
        : BWindow(frame, "Web Inspector", B_TITLED_WINDOW, B_ASYNCHRONOUS_CONTROLS | B_QUIT_ON_WINDOW_CLOSE)
    {
    }

    void MessageReceived(BMessage* message) override
    {
        switch (message->what) {
        default:
            BWindow::MessageReceived(message);
            break;
        }
    }
};

WebPageProxy* RemoteWebInspectorUIProxy::platformCreateFrontendPageAndWindow()
{
    BRect rect(100, 100, 900, 700);
    InspectorWindow* window = new InspectorWindow(rect);

    Ref<API::PageConfiguration> configuration = API::PageConfiguration::create();
    auto webView = WebViewBase::create("InspectorView", window->Bounds(), window, configuration.get());
    auto* page = webView->page();
    window->AddChild(webView.leakRef());
    window->Show();

    return page;
}

void RemoteWebInspectorUIProxy::platformCloseFrontendPageAndWindow()
{
    if (m_inspectorPage)
        m_inspectorPage->close();
}

void RemoteWebInspectorUIProxy::platformResetState()
{
    // TODO: Reset any persisted state if necessary.
}

void RemoteWebInspectorUIProxy::platformBringToFront()
{
    if (m_inspectorPage) {
        // Accessing the view/window from the page proxy might be indirect.
        // Assuming we can find the window somehow or track it.
        // For now, this is a best effort stub or requires storing the window pointer.
        // But m_inspectorPage is a WebPageProxy.
        // We don't easily have access to the BWindow created in platformCreateFrontendPageAndWindow
        // unless we store it.
        // For now, we leave it as is, or we could add a member to track the window.
    }
}

void RemoteWebInspectorUIProxy::platformSave(Vector<WebCore::InspectorFrontendClient::SaveData>&& saveData, bool forceSaveAs)
{
    // Simple implementation: save to a default location or show a file panel.
    // Since we can't easily block for a file panel here without more infrastructure,
    // we'll save to a fixed location or just log.
    // A proper implementation would use BFilePanel.

    // For now, just a stub that acknowledges the request.
    (void)saveData;
    (void)forceSaveAs;
}

void RemoteWebInspectorUIProxy::platformLoad(const String& path, CompletionHandler<void(const String&)>&& completionHandler)
{
    BFile file(path.utf8().data(), B_READ_ONLY);
    if (file.InitCheck() != B_OK) {
        completionHandler(String());
        return;
    }

    off_t size;
    file.GetSize(&size);

    auto buffer = makeUniqueArray<char>(size + 1);
    if (file.Read(buffer.get(), size) != size) {
        completionHandler(String());
        return;
    }
    buffer[size] = '\0';

    completionHandler(String::fromUTF8(buffer.get()));
}

void RemoteWebInspectorUIProxy::platformPickColorFromScreen(CompletionHandler<void(const std::optional<WebCore::Color>&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void RemoteWebInspectorUIProxy::platformSetSheetRect(const WebCore::FloatRect&)
{
}

void RemoteWebInspectorUIProxy::platformSetForcedAppearance(WebCore::InspectorFrontendClient::Appearance)
{
}

void RemoteWebInspectorUIProxy::platformStartWindowDrag()
{
}

void RemoteWebInspectorUIProxy::platformOpenURLExternally(const String& url)
{
    const char* argv[] = { url.utf8().data(), nullptr };
    be_roster->Launch("text/html", 1, const_cast<char**>(argv));
}

void RemoteWebInspectorUIProxy::platformRevealFileExternally(const String& path)
{
    entry_ref ref;
    if (get_ref_for_path(path.utf8().data(), &ref) == B_OK) {
        BMessage msg(B_REFS_RECEIVED);
        msg.AddRef("refs", &ref);
        be_roster->Launch("application/x-vnd.Be-TRAK", &msg);
    }
}

void RemoteWebInspectorUIProxy::platformShowCertificate(const WebCore::CertificateInfo&)
{
    BAlert* alert = new BAlert("Certificate Info", "Certificate viewing is not yet implemented.", "OK");
    alert->Go(nullptr);
}

} // namespace WebKit
