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
#include "APIInfo.h"
#include "APIInfo.h"
#include "APIOpenPanelParameters.h"
#include "WebOpenPanelResultListenerProxy.h"
#include <WebCore/FloatSize.h>
#include <WebCore/NotificationData.h>
#include <WebCore/NotificationResources.h>
#include <FilePanel.h>
#include <Notification.h>
#include <PrintJob.h>
#include <Rect.h>
#include <Window.h>
#include <cmath>

namespace WebKit {

class OpenPanelHandler : public BHandler {
public:
    OpenPanelHandler(Ref<WebOpenPanelResultListenerProxy>&& listener, bool allowMultiple)
        : BHandler("OpenPanelHandler")
        , m_listener(WTF::move(listener))
        , m_panel(new BFilePanel(B_OPEN_PANEL, NULL, NULL, allowMultiple ? B_FILE_NODE : B_FILE_NODE | B_DIRECTORY_NODE, allowMultiple))
    {
        m_panel->SetTarget(this);
    }

    virtual ~OpenPanelHandler()
    {
        delete m_panel;
    }

    void Show()
    {
        m_panel->Show();
    }

    void MessageReceived(BMessage* message) override
    {
        switch (message->what) {
            case B_REFS_RECEIVED: {
                entry_ref ref;
                Vector<String> filenames;
                for (int32 i = 0; message->FindRef("refs", i, &ref) == B_OK; i++) {
                    BEntry entry(&ref);
                    BPath path;
                    if (entry.GetPath(&path) == B_OK)
                        filenames.append(String::fromUTF8(path.Path()));
                }
                m_listener->chooseFiles(filenames);
                delete this;
                break;
            }
            case B_CANCEL:
                m_listener->cancel();
                delete this;
                break;
            default:
                BHandler::MessageReceived(message);
        }
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

void PageUIClientHaiku::runOpenPanel(WebPageProxy&, WebFrameProxy&, const WebCore::SecurityOriginData&, API::OpenPanelParameters* parameters, WebOpenPanelResultListenerProxy* listener)
{
    // BFilePanel is async, so we create a handler that deletes itself upon completion.
    // Ideally we should attach it to the WebView's window loop, but a floating handler might work if looped correctly.
    // Actually, BFilePanel runs in its own thread/looper usually, but sends messages to target.
    // If target is looperless, it might be an issue.
    // Let's attach the handler to the WebView's window if possible.

    // Note: This implementation is a bit simplified and assumes the handler stays alive until callback.
    // BFilePanel takes ownership of nothing, but we delete handler in MessageReceived.

    // We need to ensure the listener is kept alive.
    if (!listener) return;

    // Run on main thread?
    // Listener proxy calls back via IPC.

    bool allowMultiple = parameters->allowMultipleFiles();
    auto handler = new OpenPanelHandler(Ref { *listener }, allowMultiple);

    if (m_webView.Window()) {
        m_webView.Window()->AddHandler(handler);
    } else {
        // Fallback or leak? If no window, we can't really attach easily without a looper.
        // Maybe create a looper?
        // For now assume WebView is attached.
        delete handler;
        listener->cancel();
        return;
    }

    handler->Show();
}

void PageUIClientHaiku::showNotification(WebPageProxy&, const WebCore::NotificationData& data, RefPtr<WebCore::NotificationResources>&&, CompletionHandler<void()>&& completionHandler)
{
    BNotification notification(B_INFORMATION_NOTIFICATION);
    notification.SetTitle(data.title.utf8().data());
    notification.SetContent(data.body.utf8().data());
    // TODO: Handle icon from resources if available

    notification.Send();
    completionHandler();
}

} // namespace WebKit
