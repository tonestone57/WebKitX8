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
#include "WebContextMenuProxyHaiku.h"

#if ENABLE(CONTEXT_MENUS)

#include "WebContextMenuItem.h"
#include "WebContextMenuItemData.h"
#include "WebPageProxy.h"
#include "WebViewBase.h"

#include <InterfaceDefs.h>
#include <MenuItem.h>
#include <Message.h>
#include <PopUpMenu.h>
#include <View.h>
#include <Window.h>

namespace WebKit {

WebContextMenuProxyHaiku::WebContextMenuProxyHaiku(WebViewBase& webView, WebPageProxy& page, FrameInfoData&& frameInfo, ContextMenuContextData&& context, const UserData& userData)
    : WebContextMenuProxy(page, WTF::move(frameInfo), WTF::move(context), userData)
    , m_webView(webView)
    , m_menu(nullptr)
{
}

WebContextMenuProxyHaiku::~WebContextMenuProxyHaiku()
{
    if (m_menu) {
        delete m_menu;
        m_menu = nullptr;
    }
}

void WebContextMenuProxyHaiku::populateMenu(BMenu* menu, const Vector<WebContextMenuItemData>& items)
{
    for (const auto& item : items) {
        switch (item.type()) {
            case WebCore::ContextMenuItemType::Separator:
                menu->AddSeparatorItem();
                break;
            case WebCore::ContextMenuItemType::Submenu: {
                BMenu* submenu = new BMenu(item.title().utf8().data());
                populateMenu(submenu, item.submenu());

                BMenuItem* menuItem = new BMenuItem(submenu);
                menuItem->SetEnabled(item.enabled());
                menu->AddItem(menuItem);
                break;
            }
            case WebCore::ContextMenuItemType::Action:
            case WebCore::ContextMenuItemType::CheckableAction: {
                // We store the pointer to the item data in the message.
                // Since this function runs synchronously during showContextMenuWithItems,
                // the data will remain valid until the menu closes.
                // However, Vector reallocation could invalidate pointers if we were storing pointers to elements of a local vector that grows.
                // But here 'items' is const ref to a vector that is part of the recursion.
                // Actually, the safest way is to store a unique ID or index in a flat list.
                // But since showContextMenuWithItems is blocking, we can't easily use a member flat list without clearing it.
                // Let's use a pointer to the WebContextMenuItemData, but we must ensure it lives long enough.
                // The root vector is in showContextMenuWithItems. Submenu vectors are inside WebContextMenuItemData.
                // As long as the root vector lives, all sub-vectors live.

                BMessage* message = new BMessage('cxtm');
                message->AddPointer("data", &item);

                BMenuItem* menuItem = new BMenuItem(item.title().utf8().data(), message);
                menuItem->SetEnabled(item.enabled());
                if (item.checked())
                    menuItem->SetMarked(true);
                menu->AddItem(menuItem);
                break;
            }
            default:
                break;
        }
    }
}

void WebContextMenuProxyHaiku::showContextMenuWithItems(Vector<Ref<WebContextMenuItem>>&& items)
{
    if (m_menu) {
        delete m_menu;
        m_menu = nullptr;
    }

    if (items.isEmpty())
        return;

    // We need to keep the data alive while the menu runs.
    // We convert Ref<WebContextMenuItem> to WebContextMenuItemData vector to handle ownership if needed,
    // but actually WebContextMenuItem holds the data. Ref keeps it alive.
    // 'items' is passed by rvalue ref, so we own it.
    // We can just keep 'items' alive in this scope.

    // However, populateMenu expects Vector<WebContextMenuItemData>.
    // We need to extract data from Ref<WebContextMenuItem>.
    Vector<WebContextMenuItemData> rootItems;
    for (const auto& item : items)
        rootItems.append(item->data());

    // NOTE: 'rootItems' is a local vector. Pointers to its elements are stable ONLY if it doesn't reallocate.
    // But 'rootItems' won't reallocate after population.
    // Submenu items are inside WebContextMenuItemData, which are inside rootItems.
    // Since WebContextMenuItemData owns its submenus (Vector<WebContextMenuItemData>),
    // pointers to those inner elements are also stable as long as rootItems is not modified.

    m_menu = new BPopUpMenu("ContextMenu");
    m_menu->SetRadioMode(false); // Context menus usually don't behave like radio groups unless specified

    populateMenu(m_menu, rootItems);

    if (!m_webView.LockLooper()) return;

    // Haiku rects are left, top, right, bottom (inclusive).
    WebCore::IntPoint location = m_context.menuLocation();
    BPoint screenPoint(location.x(), location.y());
    m_webView.ConvertToScreen(&screenPoint);

    // Offset slightly so mouse is not directly on top of first item?
    // BPopUpMenu::Go handles placement.

    m_webView.UnlockLooper();

    BMenuItem* selectedItem = m_menu->Go(screenPoint, false, false);

    if (selectedItem) {
        BMessage* msg = selectedItem->Message();
        if (msg) {
            WebContextMenuItemData* itemData;
            if (msg->FindPointer("data", (void**)&itemData) == B_OK && itemData) {
                if (page())
                    page()->contextMenuItemSelected(*itemData, frameInfo());
            }
        }
    }

    // Clean up is handled by destructor or next call, but we can clean up now.
    delete m_menu;
    m_menu = nullptr;
}

} // namespace WebKit

#endif // ENABLE(CONTEXT_MENUS)
