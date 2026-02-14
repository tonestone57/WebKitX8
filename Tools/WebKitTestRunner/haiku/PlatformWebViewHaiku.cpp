/*
 * Copyright (C) 2014 Haiku, inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformWebView.h"

#include "APICast.h"
#include "APIPageConfiguration.h"
#include "WebViewBase.h"
#include <Window.h>

namespace WTR {

PlatformWebView::PlatformWebView(WKPageConfigurationRef configuration, const TestOptions& options)
    : m_options(options)
{
    BRect frame(0, 0, 800, 600);
    m_window = new BWindow(frame, "WebKitTestRunner", B_TITLED_WINDOW, B_NOT_RESIZABLE | B_QUIT_ON_WINDOW_CLOSE);

    auto& config = *reinterpret_cast<API::PageConfiguration*>(configuration);
    m_view = WebKit::WebViewBase::create("WTRView", frame, m_window, config);

    m_window->AddChild(m_view.get());
    m_window->Show();
}

PlatformWebView::~PlatformWebView()
{
    if (m_window) {
        if (m_window->Lock()) {
            if (m_view && m_view->Window())
                m_view->RemoveSelf();
            m_window->Quit();
        }
    }
}

void PlatformWebView::resizeTo(unsigned width, unsigned height, WebViewSizingMode)
{
    if (m_window)
        m_window->ResizeTo(width, height);
}

WKPageRef PlatformWebView::page()
{
    return toAPI(m_view->page());
}

void PlatformWebView::focus()
{
    if (m_view->LockLooper()) {
        m_view->MakeFocus(true);
        m_view->UnlockLooper();
    }
}

WKRect PlatformWebView::windowFrame()
{
    if (!m_window)
        return WKRectMake(0, 0, 0, 0);

    BRect frame = m_window->Frame();
    return WKRectMake(frame.left, frame.top, frame.Width(), frame.Height());
}

void PlatformWebView::setWindowFrame(WKRect frame, WebViewSizingMode)
{
    if (m_window) {
        m_window->MoveTo(frame.origin.x, frame.origin.y);
        m_window->ResizeTo(frame.size.width, frame.size.height);
    }
}

void PlatformWebView::didInitializeClients()
{
}

void PlatformWebView::addChromeInputField()
{
}

void PlatformWebView::removeChromeInputField()
{
}

void PlatformWebView::makeWebViewFirstResponder()
{
    focus();
}

void PlatformWebView::changeWindowScaleIfNeeded(float)
{
}

void PlatformWebView::setNavigationGesturesEnabled(bool)
{
}

void PlatformWebView::forceWindowFramesChanged()
{
}

void PlatformWebView::setWindowIsKey(bool isKey)
{
    m_windowIsKey = isKey;
    if (isKey && m_window)
        m_window->Activate();
}

void PlatformWebView::setTextInChromeInputField(const String&)
{
}

void PlatformWebView::selectChromeInputField()
{
}

String PlatformWebView::getSelectedTextInChromeInputField()
{
    return String();
}

bool PlatformWebView::isSecureEventInputEnabled() const
{
    return false;
}

bool PlatformWebView::drawsBackground() const
{
    return true;
}

void PlatformWebView::setDrawsBackground(bool)
{
}

void PlatformWebView::setEditable(bool)
{
}

void PlatformWebView::removeFromWindow()
{
    if (m_view && m_view->Window())
        m_view->RemoveSelf();
}

void PlatformWebView::addToWindow()
{
    if (m_view && !m_view->Window() && m_window)
        m_window->AddChild(m_view.get());
}

PlatformImage PlatformWebView::windowSnapshotImage()
{
    return nullptr;
}

} // namespace WTR
