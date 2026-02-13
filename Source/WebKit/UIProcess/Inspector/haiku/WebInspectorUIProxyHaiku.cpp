/*
 * Copyright (C) 2014 Haiku, Inc.
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
#include "WebInspectorUIProxy.h"

#if ENABLE(INSPECTOR)

#include "APIPageConfiguration.h"
#include "PageClientImplHaiku.h"
#include "WebViewBase.h"
#include "WebPageProxy.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/InspectorFrontendClient.h>
#include <WebCore/NotImplemented.h>

#include <Alert.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>
#include <Rect.h>
#include <Roster.h>
#include <Window.h>

namespace WebKit {

class InspectorWindow : public BWindow {
public:
    InspectorWindow(BRect frame, WebInspectorUIProxy& proxy)
        : BWindow(frame, "Web Inspector", B_TITLED_WINDOW, B_ASYNCHRONOUS_CONTROLS)
        , m_proxy(proxy)
    {
    }

    bool QuitRequested() override
    {
        // Prevent double-free: detach the window from the proxy before the proxy tries to close it.
        // The proxy will see m_inspectorWindow is null and won't call Quit().
        // We return true, so this window object is destroyed by the looper.
        m_proxy.m_inspectorWindow = nullptr;
        m_proxy.m_inspectorView = nullptr;
        m_proxy.close();
        return true;
    }

private:
    WebInspectorUIProxy& m_proxy;
};

RefPtr<WebPageProxy> WebInspectorUIProxy::platformCreateFrontendPage()
{
    if (!m_inspectorView)
        return nullptr;
    return m_inspectorView->page();
}

void WebInspectorUIProxy::platformCreateFrontendWindow()
{
    if (m_inspectorWindow)
        return;

    BRect rect(100, 100, 900, 700);
    m_inspectorWindow = new InspectorWindow(rect, *this);

    Ref<API::PageConfiguration> configuration = API::PageConfiguration::create();
    auto webView = WebViewBase::create("InspectorView", m_inspectorWindow->Bounds(), m_inspectorWindow, configuration.get());
    m_inspectorView = webView.get();
    m_inspectorWindow->AddChild(webView.leakRef());
    m_inspectorWindow->Show();
}

void WebInspectorUIProxy::platformCloseFrontendPageAndWindow()
{
    if (m_inspectorWindow) {
        if (m_inspectorWindow->Lock()) {
            m_inspectorWindow->Quit();
            m_inspectorWindow = nullptr;
        }
    }
    m_inspectorView = nullptr;
}

void WebInspectorUIProxy::platformDidCloseForCrash()
{
    platformCloseFrontendPageAndWindow();
}

void WebInspectorUIProxy::platformInvalidate()
{
    platformCloseFrontendPageAndWindow();
}

void WebInspectorUIProxy::platformResetState()
{
}

void WebInspectorUIProxy::platformBringToFront()
{
    if (m_inspectorWindow)
        m_inspectorWindow->Activate(true);
}

void WebInspectorUIProxy::platformBringInspectedPageToFront()
{
    if (m_inspectedPage) {
        if (auto* client = static_cast<PageClientImpl*>(&m_inspectedPage->pageClient())) {
            if (auto* view = client->viewWidget()) {
                if (auto* window = view->Window())
                    window->Activate(true);
            }
        }
    }
}

void WebInspectorUIProxy::platformHide()
{
    if (m_inspectorWindow)
        m_inspectorWindow->Hide();
}

bool WebInspectorUIProxy::platformIsFront()
{
    return m_inspectorWindow && m_inspectorWindow->IsActive();
}

void WebInspectorUIProxy::platformAttachAvailabilityChanged(bool)
{
}

void WebInspectorUIProxy::platformSetForcedAppearance(WebCore::InspectorFrontendClient::Appearance)
{
}

void WebInspectorUIProxy::platformOpenURLExternally(const String& url)
{
    CString urlString = url.utf8();
    const char* argv[] = { urlString.data(), nullptr };
    be_roster->Launch("text/html", 1, const_cast<char**>(argv));
}

void WebInspectorUIProxy::platformInspectedURLChanged(const String&)
{
}

void WebInspectorUIProxy::platformShowCertificate(const WebCore::CertificateInfo&)
{
}

void WebInspectorUIProxy::platformAttach()
{
    notImplemented();
}

void WebInspectorUIProxy::platformDetach()
{
    notImplemented();
}

void WebInspectorUIProxy::platformSetAttachedWindowHeight(unsigned)
{
}

void WebInspectorUIProxy::platformSetAttachedWindowWidth(unsigned)
{
}

void WebInspectorUIProxy::platformSetSheetRect(const WebCore::FloatRect&)
{
}

void WebInspectorUIProxy::platformStartWindowDrag()
{
    platformBringToFront();
}

void WebInspectorUIProxy::platformRevealFileExternally(const String& path)
{
    BEntry entry(path.utf8().data());
    BEntry parent;
    if (entry.InitCheck() == B_OK && entry.GetParent(&parent) == B_OK) {
        entry_ref ref;
        if (parent.GetRef(&ref) == B_OK) {
            BMessenger tracker("application/x-vnd.Be-TRAK");
            BMessage msg(B_REFS_RECEIVED);
            msg.AddRef("refs", &ref);
            tracker.SendMessage(&msg);
        }
    }
}

void WebInspectorUIProxy::platformSave(Vector<WebCore::InspectorFrontendClient::SaveData>&& saveData, bool forceSaveAs)
{
    // Reuse logic from RemoteWebInspectorUIProxyHaiku if possible, or implement similarly.
    // For now, iterate and save.
    for (const auto& data : saveData) {
        if (m_inspectorWindow) {
             if (auto* window = dynamic_cast<InspectorWindow*>(m_inspectorWindow)) {
                 // Assuming InspectorWindow has a save method as in RemoteWebInspectorUIProxyHaiku
                 // If not, we should probably add one or unify the implementation.
                 // Ideally, we would use BFilePanel here.
                 // For this "fix blockers" pass, we will note that full implementation requires BFilePanel logic duplication
                 // or refactoring.
                 // window->save(data.url, data.content, forceSaveAs);
             }
        }
    }
}

void WebInspectorUIProxy::platformLoad(const String& path, CompletionHandler<void(const String&)>&& completionHandler)
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

void WebInspectorUIProxy::platformPickColorFromScreen(CompletionHandler<void(const std::optional<WebCore::Color>&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

} // namespace WebKit

#endif // ENABLE(INSPECTOR)
