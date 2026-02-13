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
#include "WebViewConstants.h"

#include "APIFrameInfo.h"
#include "APINavigationAction.h"
#include "APIPageConfiguration.h"
#include "WebEvent.h"
#include "WebHitTestResultData.h"
#include "WebPageProxy.h"

#include <WebCore/FloatRect.h>
#include <WebCore/FloatSize.h>
#include <Alert.h>
#include <PrintJob.h>
#include <Rect.h>
#include <Window.h>
#include <cmath>

namespace WebKit {

PageUIClientHaiku::PageUIClientHaiku(WebViewBase& webView)
    : m_webView(webView)
{
}

PageUIClientHaiku::~PageUIClientHaiku()
{
}

void PageUIClientHaiku::createNewPage(WebPageProxy& page, Ref<API::PageConfiguration>&& configuration, Ref<API::NavigationAction>&& navigationAction, CompletionHandler<void(RefPtr<WebPageProxy>&&)>&& completionHandler)
{
    if (BWindow* window = m_webView.Window()) {
        BMessage message(CREATE_NEW_PAGE);
        window->PostMessage(&message);
    }
    completionHandler(nullptr);
}

void PageUIClientHaiku::showPage(WebPageProxy* page)
{
    if (BWindow* window = m_webView.Window())
        window->PostMessage(SHOW_PAGE);
}

void PageUIClientHaiku::close(WebPageProxy* page)
{
    if (BWindow* window = m_webView.Window())
        window->PostMessage(CLOSE_PAGE);
}

void PageUIClientHaiku::runJavaScriptAlert(WebPageProxy& page, const WTF::String& message, WebFrameProxy* frame, FrameInfoData&& frameInfo, Function<void()>&& completionHandler)
{
    if (BWindow* window = m_webView.Window()) {
        // Ensure we don't block the window thread if we are on it?
        // BAlert::Go() blocks.
        // But this is expected behavior for alerts.
        BAlert* alert = new BAlert("JavaScript Alert", message.utf8().data(), "OK");
        alert->Go();
    }
    completionHandler();
}

void PageUIClientHaiku::runJavaScriptConfirm(WebPageProxy& page, const WTF::String& message, WebFrameProxy* frame, FrameInfoData&& frameInfo, Function<void(bool)>&& completionHandler)
{
    bool result = false;
    if (BWindow* window = m_webView.Window()) {
        BAlert* alert = new BAlert("JavaScript Confirm", message.utf8().data(), "Cancel", "OK");
        result = (alert->Go() == 1);
    }
    completionHandler(result);
}

void PageUIClientHaiku::runJavaScriptPrompt(WebPageProxy& page, const WTF::String& message, const WTF::String& defaultValue, WebFrameProxy* frame, FrameInfoData&& frameInfo, Function<void(const WTF::String&)>&& completionHandler)
{
    // FIXME: Implement a prompt dialog
    completionHandler(WTF::String());
}

void PageUIClientHaiku::setStatusText(WebPageProxy* page, const WTF::String& text)
{
    if (BWindow* window = m_webView.Window()) {
        BMessage message(SET_STATUS_TEXT);
        message.AddString("text", text.utf8().data());
        window->PostMessage(&message);
    }
}

void PageUIClientHaiku::mouseDidMoveOverElement(WebPageProxy& page, const WebHitTestResultData& hitTestResult, OptionSet<WebEventModifier> modifiers)
{
    if (BWindow* window = m_webView.Window()) {
        BMessage message(MOUSE_DID_MOVE_OVER_ELEMENT);
        message.AddString("url", hitTestResult.absoluteImageURL.string().utf8().data());
        message.AddString("linkUrl", hitTestResult.absoluteLinkURL.string().utf8().data());
        window->PostMessage(&message);
    }
}

void PageUIClientHaiku::toolbarsAreVisible(WebPageProxy&, Function<void(bool)>&& completionHandler)
{
    completionHandler(true);
}

void PageUIClientHaiku::setToolbarsAreVisible(WebPageProxy&, bool visible)
{
    if (BWindow* window = m_webView.Window()) {
        BMessage message(TOOLBARS_VISIBILITY_CHANGED);
        message.AddBool("visible", visible);
        window->PostMessage(&message);
    }
}

void PageUIClientHaiku::menuBarIsVisible(WebPageProxy&, Function<void(bool)>&& completionHandler)
{
    completionHandler(true);
}

void PageUIClientHaiku::setMenuBarIsVisible(WebPageProxy&, bool visible)
{
    if (BWindow* window = m_webView.Window()) {
        BMessage message(MENU_BAR_VISIBILITY_CHANGED);
        message.AddBool("visible", visible);
        window->PostMessage(&message);
    }
}

void PageUIClientHaiku::statusBarIsVisible(WebPageProxy&, Function<void(bool)>&& completionHandler)
{
    completionHandler(true);
}

void PageUIClientHaiku::setStatusBarIsVisible(WebPageProxy&, bool visible)
{
    if (BWindow* window = m_webView.Window()) {
        BMessage message(STATUS_BAR_VISIBILITY_CHANGED);
        message.AddBool("visible", visible);
        window->PostMessage(&message);
    }
}

void PageUIClientHaiku::setIsResizable(WebPageProxy&, bool resizable)
{
    if (BWindow* window = m_webView.Window()) {
        BMessage message(RESIZABLE_CHANGED);
        message.AddBool("resizable", resizable);
        window->PostMessage(&message);
    }
}

void PageUIClientHaiku::setWindowFrame(WebPageProxy&, const WebCore::FloatRect& frame)
{
    if (BWindow* window = m_webView.Window()) {
        BMessage message(WINDOW_FRAME_CHANGED);
        message.AddRect("frame", BRect(frame));
        window->PostMessage(&message);

        if (window->Lock()) {
            window->MoveTo(frame.x(), frame.y());
            window->ResizeTo(frame.width(), frame.height());
            window->Unlock();
        }
    }
}

void PageUIClientHaiku::windowFrame(WebPageProxy&, Function<void(WebCore::FloatRect)>&& completionHandler)
{
    WebCore::FloatRect frame;
    if (BWindow* window = m_webView.Window()) {
        if (window->Lock()) {
            frame = WebCore::FloatRect(window->Frame());
            window->Unlock();
        } else {
             frame = WebCore::FloatRect(window->Frame());
        }
    }
    completionHandler(frame);
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

} // namespace WebKit
