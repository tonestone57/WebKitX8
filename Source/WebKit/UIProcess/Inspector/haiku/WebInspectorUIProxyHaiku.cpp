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
#include <WebCore/Color.h>

#include <Alert.h>
#include <Bitmap.h>
#include <Cursor.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FilePanel.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>
#include <Rect.h>
#include <Roster.h>
#include <Screen.h>
#include <Window.h>

namespace WebKit {

class InspectorWindow : public BWindow {
public:
    InspectorWindow(BRect frame, WebInspectorUIProxy& proxy)
        : BWindow(frame, "Web Inspector", B_TITLED_WINDOW, B_ASYNCHRONOUS_CONTROLS)
        , m_proxy(proxy)
        , m_filePanel(nullptr)
    {
    }

    ~InspectorWindow()
    {
        delete m_filePanel;
        if (m_pickingColor)
            cancelColorPicking();
    }

    bool QuitRequested() override
    {
        if (m_pickingColor)
            cancelColorPicking();
        // Prevent double-free: detach the window from the proxy before the proxy tries to close it.
        // The proxy will see m_inspectorWindow is null and won't call Quit().
        // We return true, so this window object is destroyed by the looper.
        m_proxy.m_inspectorWindow = nullptr;
        m_proxy.m_inspectorView = nullptr;
        m_proxy.close();
        return true;
    }

    void DispatchMessage(BMessage* message, BHandler* handler) override
    {
        if (m_pickingColor) {
            if (message->what == B_MOUSE_DOWN) {
                handleColorPick(message);
                return;
            }
            if (message->what == B_KEY_DOWN) {
                 int32 key;
                 if (message->FindInt32("key", &key) == B_OK && key == B_ESCAPE) {
                     cancelColorPicking();
                     return;
                 }
            }
        }
        BWindow::DispatchMessage(message, handler);
    }

    void MessageReceived(BMessage* message) override
    {
        switch (message->what) {
        case B_SAVE_REQUESTED:
            handleSaveRequest(message);
            break;
        default:
            BWindow::MessageReceived(message);
            break;
        }
    }

    void startColorPicking(CompletionHandler<void(const std::optional<WebCore::Color>&)>&& handler)
    {
        if (m_colorPickingHandler)
            m_colorPickingHandler(std::nullopt);

        m_colorPickingHandler = WTFMove(handler);

        BCursor cursor(B_CURSOR_ID_CROSS_HAIR);
        if (auto* view = ChildAt(0)) {
            if (Lock()) {
                view->SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS | B_NO_POINTER_HISTORY);
                view->SetViewCursor(&cursor);
                m_pickingColor = true;
                Unlock();
            }
        }
    }

    void cancelColorPicking()
    {
        m_pickingColor = false;
        if (Lock()) {
            if (auto* view = ChildAt(0)) {
                view->SetMouseEventMask(0);
                view->SetViewCursor(B_CURSOR_SYSTEM_DEFAULT);
            }
            Unlock();
        }
        if (m_colorPickingHandler)
            m_colorPickingHandler(std::nullopt);
    }

    void save(const String& suggestedURL, const String& content, bool forceSaveAs)
    {
        if (!m_filePanel)
            m_filePanel = new BFilePanel(B_SAVE_PANEL, new BMessenger(this));

        BMessage* message = new BMessage(B_SAVE_REQUESTED);
        message->AddString("content", content.utf8().data());
        m_filePanel->SetMessage(message);

        if (!suggestedURL.isEmpty())
            m_filePanel->SetSaveText(suggestedURL.utf8().data());

        m_filePanel->Show();
    }

private:
    void handleSaveRequest(BMessage* message)
    {
        entry_ref ref;
        const char* name;
        const char* content;
        if (message->FindRef("directory", &ref) == B_OK
            && message->FindString("name", &name) == B_OK
            && message->FindString("content", &content) == B_OK) {
            BDirectory dir(&ref);
            BFile file(&dir, name, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
            if (file.InitCheck() == B_OK)
                file.Write(content, strlen(content));
        }
    }

    void handleColorPick(BMessage* message)
    {
        BPoint screenPoint;
        if (message->FindPoint("screen_where", &screenPoint) != B_OK) {
             // Fallback
             BPoint where;
             if (message->FindPoint("where", &where) == B_OK) {
                 if (auto* view = ChildAt(0)) {
                     if (Lock()) {
                         screenPoint = view->ConvertToScreen(where);
                         Unlock();
                     }
                 }
             }
        }

        BScreen screen(this);
        rgb_color color = screen.DesktopColor();

        BBitmap bitmap(BRect(0, 0, 0, 0), B_RGB32);
        if (screen.ReadBitmap(&bitmap, false, &BRect(screenPoint.x, screenPoint.y, screenPoint.x, screenPoint.y)) == B_OK) {
             uint8* bits = (uint8*)bitmap.Bits();
             color.blue = bits[0];
             color.green = bits[1];
             color.red = bits[2];
             color.alpha = bits[3];
        }

        if (m_colorPickingHandler) {
             // WebCore::Color expects SRGBA
             m_colorPickingHandler(WebCore::Color(WebCore::SRGBA<uint8_t> { color.red, color.green, color.blue, color.alpha }));
             // Prevent calling it again in cancelColorPicking
             m_colorPickingHandler = nullptr;
        }

        cancelColorPicking();
    }

    WebInspectorUIProxy& m_proxy;
    BFilePanel* m_filePanel;
    CompletionHandler<void(const std::optional<WebCore::Color>&)> m_colorPickingHandler;
    bool m_pickingColor { false };
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
    // Attachment requires embedding the inspector view into the page's window.
    // This is currently not supported by the Haiku MiniBrowser architecture.
}

void WebInspectorUIProxy::platformDetach()
{
    // Detachment usually involves creating a new window for the inspector.
    // Since we only support separate window mode, this is a no-op or handled by close/open.
}

void WebInspectorUIProxy::platformSetAttachedWindowHeight(unsigned)
{
    // Not supported for detached window.
}

void WebInspectorUIProxy::platformSetAttachedWindowWidth(unsigned)
{
    // Not supported for detached window.
}

void WebInspectorUIProxy::platformSetSheetRect(const WebCore::FloatRect&)
{
    // Not supported.
}

void WebInspectorUIProxy::platformStartWindowDrag()
{
    if (m_inspectorWindow) {
        if (m_inspectorWindow->Lock()) {
            // Initiate window dragging if mouse is down.
            // BWindow handles dragging via B_WINDOW_MOVE messages usually,
            // or we just let standard window manager decorations handle it.
            // If this is triggered from web content (e.g. custom titlebar), we might need to simulate drag.
            // For now, activating is a safe fallback.
            m_inspectorWindow->Activate(true);
            m_inspectorWindow->Unlock();
        }
    }
}

void WebInspectorUIProxy::platformRevealFileExternally(const String& path)
{
    entry_ref ref;
    if (get_ref_for_path(path.utf8().data(), &ref) == B_OK) {
        // We want to open the *folder* containing the file and select it.
        // Haiku's Tracker handles B_REFS_RECEIVED by opening the folder.
        // If we pass the file ref, it might open the file (execute/edit).
        // To reveal, usually we open parent and select child.

        BEntry entry(&ref);
        BEntry parent;
        if (entry.GetParent(&parent) == B_OK) {
            entry_ref parentRef;
            if (parent.GetRef(&parentRef) == B_OK) {
                 BMessage msg(B_REFS_RECEIVED);
                 msg.AddRef("refs", &parentRef);
                 // TODO: Select the specific file in the folder (requires scripting Tracker)
                 be_roster->Launch("application/x-vnd.Be-TRAK", &msg);
            }
        }
    }
}

void WebInspectorUIProxy::platformSave(Vector<WebCore::InspectorFrontendClient::SaveData>&& saveData, bool forceSaveAs)
{
    for (const auto& data : saveData) {
        if (m_inspectorWindow) {
             if (auto* window = dynamic_cast<InspectorWindow*>(m_inspectorWindow)) {
                 window->save(data.url, data.content, forceSaveAs);
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
    if (m_inspectorWindow) {
        if (auto* window = dynamic_cast<InspectorWindow*>(m_inspectorWindow)) {
            window->startColorPicking(WTFMove(completionHandler));
            return;
        }
    }
    completionHandler(std::nullopt);
}

} // namespace WebKit

#endif // ENABLE(INSPECTOR)
