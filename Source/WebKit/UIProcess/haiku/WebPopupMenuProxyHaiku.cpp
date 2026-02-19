/*
 * Copyright (C) 2014, 2024 Haiku, Inc
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
#include "WebPopupMenuProxyHaiku.h"

#include "PageClientImplHaiku.h"
#include "WebPopupItem.h"
#include "../../API/haiku/WebViewBase.h"
#include <WebCore/IntRect.h>
#include <wtf/text/WTFString.h>

#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Window.h>

namespace WebKit {

WebPopupMenuProxyHaiku::WebPopupMenuProxyHaiku(WebViewBase& webView, WebPopupMenuProxy::Client& client)
    : WebPopupMenuProxy(client)
    , m_webView(webView)
    , m_weakWebView(webView)
    , m_menu(nullptr)
{
}

WebPopupMenuProxyHaiku::~WebPopupMenuProxyHaiku()
{
    if (m_menu)
        delete m_menu;
}

void WebPopupMenuProxyHaiku::showPopupMenu(const WebCore::IntRect& rect, WebCore::TextDirection, double pageScaleFactor, const Vector<WebPopupItem>& items, const PlatformPopupMenuData&, int32_t selectedIndex)
{
    Ref<WebPopupMenuProxyHaiku> protectedThis(*this);

    if (m_menu) {
        delete m_menu;
        m_menu = nullptr;
    }

    m_menu = new BPopUpMenu("PopupMenu");
    m_menu->SetRadioMode(true);

    for (size_t i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        if (item.type == WebPopupItemType::Separator) {
            m_menu->AddSeparatorItem();
            continue;
        }

        BMessage* message = new BMessage('ppup');
        message->AddInt32("index", i);

        BMenuItem* menuItem = new BMenuItem(item.text.utf8().data(), message);
        menuItem->SetEnabled(item.isEnabled);
        if (static_cast<int32_t>(i) == selectedIndex || item.isSelected)
            menuItem->SetMarked(true);

        m_menu->AddItem(menuItem);
    }

    if (!m_webView.LockLooper()) return;

    // Haiku rects are left, top, right, bottom (inclusive).
    BRect viewRect(rect.x(), rect.y(), rect.maxX() - 1, rect.maxY() - 1);
    m_webView.ConvertToScreen(&viewRect);
    BPoint screenPoint = viewRect.LeftTop();

    m_webView.UnlockLooper();

    BMenuItem* selectedItem = m_menu->Go(screenPoint, false, true);

    if (!m_weakWebView)
        return;

    if (selectedItem) {
        BMessage* msg = selectedItem->Message();
        if (msg) {
            int32 index = msg->FindInt32("index");
            if (client()) {
                client()->valueChangedForPopupMenu(this, index);
            }
        }
    } else {
        if (client())
            client()->valueChangedForPopupMenu(this, -1);
    }

    // Clean up
    hidePopupMenu();
}

void WebPopupMenuProxyHaiku::hidePopupMenu()
{
    if (m_menu) {
        delete m_menu;
        m_menu = nullptr;
    }
}

} // namespace WebKit
