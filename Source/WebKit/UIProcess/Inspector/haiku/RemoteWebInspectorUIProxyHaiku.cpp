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
#include <Bitmap.h>
#include <Cursor.h>
#include <Entry.h>
#include <FilePanel.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>
#include <Rect.h>
#include <Roster.h>
#include <Screen.h>
#include <Window.h>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace WebKit {

class RemoteInspectorWindow : public BWindow {
public:
    RemoteInspectorWindow(BRect frame)
        : BWindow(frame, "Web Inspector", B_TITLED_WINDOW, B_ASYNCHRONOUS_CONTROLS | B_QUIT_ON_WINDOW_CLOSE)
        , m_filePanel(nullptr)
    {
    }

    ~RemoteInspectorWindow()
    {
        delete m_filePanel;
        if (m_pickingColor)
            cancelColorPicking();
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

    void save(const String& suggestedURL, const String& content, bool forceSaveAs)
    {
        m_saveContent = content;

        if (!m_filePanel)
            m_filePanel = new BFilePanel(B_SAVE_PANEL, new BMessenger(this));

        if (!suggestedURL.isEmpty())
            m_filePanel->SetSaveText(suggestedURL.utf8().data());

        m_filePanel->Show();
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

private:
    void handleSaveRequest(BMessage* message)
    {
        entry_ref ref;
        const char* name;
        if (message->FindRef("directory", &ref) == B_OK && message->FindString("name", &name) == B_OK) {
            BDirectory dir(&ref);
            BPath path(&dir, name);

            int fd = open(path.Path(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0) {
                if (flock(fd, LOCK_EX) == 0) {
                    size_t length = m_saveContent.utf8().length();
                    ssize_t written = write(fd, m_saveContent.utf8().data(), length);
                    (void)written;
                    flock(fd, LOCK_UN);
                }
                close(fd);
            }
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

    BFilePanel* m_filePanel;
    String m_saveContent;
    CompletionHandler<void(const std::optional<WebCore::Color>&)> m_colorPickingHandler;
    bool m_pickingColor { false };
};

WebPageProxy* RemoteWebInspectorUIProxy::platformCreateFrontendPageAndWindow()
{
    BRect rect(100, 100, 900, 700);
    // window will be deleted when closed
    RemoteInspectorWindow* window = new RemoteInspectorWindow(rect);

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
    for (const auto& data : saveData) {
        if (m_inspectorPage) {
             if (auto* client = static_cast<PageClientImpl*>(&m_inspectorPage->pageClient())) {
                 if (auto* view = client->viewWidget()) {
                     if (auto* window = dynamic_cast<RemoteInspectorWindow*>(view->Window())) {
                         window->save(data.url, data.content, forceSaveAs);
                     }
                 }
             }
        }
    }
}

void RemoteWebInspectorUIProxy::platformLoad(const String& path, CompletionHandler<void(const String&)>&& completionHandler)
{
    int fd = open(path.utf8().data(), O_RDONLY);
    if (fd < 0) {
        completionHandler(String());
        return;
    }

    if (flock(fd, LOCK_SH) != 0) {
        close(fd);
        completionHandler(String());
        return;
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        flock(fd, LOCK_UN);
        close(fd);
        completionHandler(String());
        return;
    }

    auto buffer = makeUniqueArray<char>(st.st_size + 1);
    ssize_t bytesRead = 0;
    while (bytesRead < st.st_size) {
        ssize_t r = read(fd, buffer.get() + bytesRead, st.st_size - bytesRead);
        if (r < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (r == 0) break;
        bytesRead += r;
    }

    flock(fd, LOCK_UN);
    close(fd);

    if (bytesRead != st.st_size) {
        completionHandler(String());
        return;
    }

    buffer[st.st_size] = '\0';
    completionHandler(String::fromUTF8(buffer.get()));
}

void RemoteWebInspectorUIProxy::platformPickColorFromScreen(CompletionHandler<void(const std::optional<WebCore::Color>&)>&& completionHandler)
{
    if (m_inspectorPage) {
         if (auto* client = static_cast<PageClientImpl*>(&m_inspectorPage->pageClient())) {
             if (auto* view = client->viewWidget()) {
                 if (auto* window = dynamic_cast<RemoteInspectorWindow*>(view->Window())) {
                     window->startColorPicking(WTFMove(completionHandler));
                     return;
                 }
             }
         }
    }
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
    platformBringToFront();
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
