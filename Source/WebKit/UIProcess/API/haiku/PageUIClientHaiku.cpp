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
#include "PageUIClientHaiku.h"

#include "WebViewBase.h"
#include <WebCore/FloatSize.h>
#include <PrintJob.h>
#include <Rect.h>
#include <Window.h>
#include <cmath>

#include "APIOpenPanelParameters.h"
#include "WebOpenPanelResultListenerProxy.h"
#include <WebCore/NotificationData.h>
#include <WebCore/NotificationResources.h>

#include <Alert.h>
#include <FilePanel.h>
#include <Notification.h>
#include <Entry.h>
#include <Path.h>
#include <Messenger.h>

namespace WebKit {

class OpenPanelHandler : public BHandler {
public:
    OpenPanelHandler(Ref<WebOpenPanelResultListenerProxy>&& listener)
        : BHandler("OpenPanelHandler")
        , m_listener(WTFMove(listener))
        , m_panel(nullptr)
    {
    }

    ~OpenPanelHandler()
    {
        delete m_panel;
    }

    void setPanel(BFilePanel* panel)
    {
        m_panel = panel;
    }

    void MessageReceived(BMessage* message) override
    {
        switch (message->what) {
        case B_REFS_RECEIVED:
        case B_SIMPLE_DATA: {
            Vector<String> filenames;
            entry_ref ref;
            for (int32 i = 0; message->FindRef("refs", i, &ref) == B_OK; i++) {
                BEntry entry(&ref, true);
                BPath path;
                if (entry.GetPath(&path) == B_OK)
                    filenames.append(String::fromUTF8(path.Path()));
            }
            m_listener->chooseFiles(filenames);
            break;
        }
        case B_CANCEL:
            m_listener->cancel();
            break;
        default:
            BHandler::MessageReceived(message);
            return;
        }

        if (Looper())
            Looper()->RemoveHandler(this);
        delete this;
    }

private:
    Ref<WebOpenPanelResultListenerProxy> m_listener;
    BFilePanel* m_panel;
};

PageUIClientHaiku::PageUIClientHaiku(WebViewBase& webView)
    : m_webView(webView)
{
}

PageUIClientHaiku::~PageUIClientHaiku()
{
}

void PageUIClientHaiku::printFrame(WebPageProxy& page, WebFrameProxy& frame, const WebCore::FloatSize& pdfFirstPageSize, CompletionHandler<void()>&& completionHandler)
{
    BPrintJob job("WebKit Print Job");

    if (job.ConfigJob() == B_OK) {
        job.BeginJob();

        // FIXME: This currently only prints the visible viewport of the WebView.
        // Full-page printing requires coordination with the WebProcess to generate
        // a PDF or render the full document content, which is not yet implemented.
        BRect printableRect = job.PrintableRect();
        BRect viewRect = m_webView.Bounds();

        // Simple scaling to fit width
        float scale = 1.0f;
        if (viewRect.Width() > printableRect.Width()) {
            scale = printableRect.Width() / viewRect.Width();
        }

        // Calculate number of pages needed for height
        float pageHeightUnscaled = printableRect.Height() / scale;
        int32 pages = static_cast<int32>(ceil(viewRect.Height() / pageHeightUnscaled));
        if (pages < 1) pages = 1;

        for (int32 i = 0; i < pages; i++) {
            BRect pageRect(0, i * pageHeightUnscaled, viewRect.Width(), (i + 1) * pageHeightUnscaled);
            if (pageRect.bottom > viewRect.Height())
                pageRect.bottom = viewRect.Height();

            if (BWindow* window = m_webView.Window()) {
                if (window->Lock()) {
                    float oldScale = m_webView.Scale();
                    m_webView.SetScale(scale);
                    // Draw the portion of the view corresponding to the current page
                    job.DrawView(&m_webView, pageRect, printableRect.LeftTop());
                    m_webView.SetScale(oldScale);
                    window->Unlock();
                }
            }
            job.SpoolPage();
        }

        job.CommitJob();
    }

    completionHandler();
}

void PageUIClientHaiku::runOpenPanel(WebPageProxy&, WebFrameProxy&, const WebCore::SecurityOriginData&, API::OpenPanelParameters& parameters, WebOpenPanelResultListenerProxy& listener)
{
    BWindow* window = m_webView.Window();
    if (!window) {
        listener.cancel();
        return;
    }

    OpenPanelHandler* handler = new OpenPanelHandler(listener);
    window->AddHandler(handler);

    uint32 nodeFlavors = B_FILE_NODE;
    if (parameters.allowDirectories())
        nodeFlavors |= B_DIRECTORY_NODE;

    BFilePanel* panel = new BFilePanel(B_OPEN_PANEL, new BMessenger(handler), nullptr, nodeFlavors, parameters.allowMultipleFiles());
    handler->setPanel(panel);
    panel->Show();
}

void PageUIClientHaiku::showNotification(WebPageProxy&, const WebCore::NotificationData& data, RefPtr<WebCore::NotificationResources>&&, CompletionHandler<void()>&& completionHandler)
{
    BNotification notification(B_INFORMATION_NOTIFICATION);
    notification.SetTitle(data.title.utf8().data());
    notification.SetContent(data.body.utf8().data());
    notification.Send();
    completionHandler();
}

void PageUIClientHaiku::runJavaScriptAlert(WebPageProxy&, const String& message, WebFrameProxy&, const WebCore::SecurityOriginData&, CompletionHandler<void()>&& completionHandler)
{
    BAlert* alert = new BAlert("JavaScript Alert", message.utf8().data(), "OK");
    alert->Go();
    completionHandler();
}

void PageUIClientHaiku::runJavaScriptConfirm(WebPageProxy&, const String& message, WebFrameProxy&, const WebCore::SecurityOriginData&, CompletionHandler<void(bool)>&& completionHandler)
{
    BAlert* alert = new BAlert("JavaScript Confirm", message.utf8().data(), "Cancel", "OK");
    int32 button = alert->Go();
    completionHandler(button == 1);
}

void PageUIClientHaiku::runJavaScriptPrompt(WebPageProxy&, const String&, const String&, WebFrameProxy&, const WebCore::SecurityOriginData&, CompletionHandler<void(const String&)>&& completionHandler)
{
    completionHandler(String());
}

} // namespace WebKit
