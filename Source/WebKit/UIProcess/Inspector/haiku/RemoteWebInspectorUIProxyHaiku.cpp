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

#if ENABLE(REMOTE_INSPECTOR)

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
#include <FilePanel.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>
#include <Rect.h>
#include <Roster.h>
#include <Window.h>

namespace WebKit {

class InspectorWindow : public BWindow {
public:
    InspectorWindow(BRect frame)
        : BWindow(frame, "Web Inspector", B_TITLED_WINDOW, B_ASYNCHRONOUS_CONTROLS | B_QUIT_ON_WINDOW_CLOSE)
        , m_saveData("")
        , m_filePanel(nullptr)
    {
    }

    ~InspectorWindow()
    {
        delete m_filePanel;
    }

    void SetSaveData(const String& data) { m_saveData = data; }

    void RequestSave(const String& filename)
    {
        if (!m_filePanel) {
            BMessenger messenger(this);
            m_filePanel = new BFilePanel(B_SAVE_PANEL, &messenger, nullptr, 0, false);
        }
        m_filePanel->SetSaveText(filename.utf8().data());
        m_filePanel->Show();
    }

    void MessageReceived(BMessage* message) override
    {
        switch (message->what) {
        case B_SAVE_REQUESTED:
        {
            entry_ref ref;
            const char* name;
            if (message->FindRef("directory", &ref) == B_OK && message->FindString("name", &name) == B_OK) {
                BDirectory dir(&ref);
                BEntry entry(&dir, name);
                BFile file(&entry, B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE);
                if (file.InitCheck() == B_OK) {
                     CString data = m_saveData.utf8();
                     file.Write(data.data(), data.length());
                }
            }
            break;
        }
        default:
            BWindow::MessageReceived(message);
            break;
        }
    }
private:
    String m_saveData;
    BFilePanel* m_filePanel;
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
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
        return;
    path.Append("WebKit/WebInspector");

    BEntry entry(path.Path());
    if (entry.Exists())
        entry.Remove();
}

void RemoteWebInspectorUIProxy::platformBringToFront()
{
    if (m_inspectorPage) {
        if (auto* client = static_cast<PageClientImpl*>(&m_inspectorPage->pageClient())) {
            if (auto* view = client->viewWidget()) {
                if (auto* window = view->Window())
                    window->Activate();
            }
        }
    }
}

void RemoteWebInspectorUIProxy::platformSave(Vector<WebCore::InspectorFrontendClient::SaveData>&& saveData, bool forceSaveAs)
{
    if (saveData.isEmpty())
        return;

    // Use the first item for now
    String data = saveData[0].content;
    String filename = saveData[0].url; // Suggested filename?

    if (m_inspectorPage) {
        if (auto* client = static_cast<PageClientImpl*>(&m_inspectorPage->pageClient())) {
            if (auto* view = client->viewWidget()) {
                if (auto* window = dynamic_cast<InspectorWindow*>(view->Window())) {
                    window->SetSaveData(data);
                    window->RequestSave(filename);
                }
            }
        }
    }
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

void RemoteWebInspectorUIProxy::platformSetSheetRect(const WebCore::FloatRect& rect)
{
    if (m_inspectorPage) {
        if (auto* client = static_cast<PageClientImpl*>(&m_inspectorPage->pageClient())) {
            if (auto* view = client->viewWidget()) {
                if (auto* window = view->Window()) {
                    if (window->Lock()) {
                        window->MoveTo(rect.x(), rect.y());
                        window->ResizeTo(rect.width(), rect.height());
                        window->Unlock();
                    }
                }
            }
        }
    }
}

void RemoteWebInspectorUIProxy::platformSetForcedAppearance(WebCore::InspectorFrontendClient::Appearance appearance)
{
    // Haiku doesn't have a direct "Force Dark Mode" API for individual windows yet in R1B4.
    // Stub implementation.
    (void)appearance;
}

void RemoteWebInspectorUIProxy::platformStartWindowDrag()
{
    if (m_inspectorPage) {
        if (auto* client = static_cast<PageClientImpl*>(&m_inspectorPage->pageClient())) {
            if (auto* view = client->viewWidget()) {
                if (auto* window = view->Window()) {
                    BPoint mousePos;
                    uint32 buttons;
                    view->GetMouse(&mousePos, &buttons);
                    BMessage dragMessage(B_SIMPLE_DATA);
                    // Standard window dragging is usually handled by the window frame.
                    // If this is for dragging the window from web content, we can use Drag().
                    // But BWindow::Drag() isn't exactly standard. BWindow::MoveBy might be better
                    // if tracked.
                    // However, WebKit usually calls this on mouse down events on header bars.
                    // Haiku windows are dragged by tabs.
                }
            }
        }
    }
}

void RemoteWebInspectorUIProxy::platformOpenURLExternally(const String& url)
{
    CString urlString = url.utf8();
    const char* argv[] = { urlString.data(), nullptr };
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

#endif // ENABLE(REMOTE_INSPECTOR)
