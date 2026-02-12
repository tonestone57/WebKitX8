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

#include "APIContextMenuClient.h"
#include "PageClient.h"
#include "WebContextMenuItem.h"
#include "WebPageProxy.h"
#include <WebCore/IntRect.h>
#include <wtf/text/WTFString.h>

#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Window.h>

namespace WebKit {

WebContextMenuProxyHaiku::WebContextMenuProxyHaiku(WebPageProxy& page, FrameInfoData&& frameInfo, ContextMenuContextData&& context, const UserData& userData)
    : WebContextMenuProxy(page, WTF::move(frameInfo), WTF::move(context), userData)
    , m_menu(nullptr)
{
}

WebContextMenuProxyHaiku::~WebContextMenuProxyHaiku()
{
    if (m_menu)
        delete m_menu;
}

static void populateMenuFromData(BMenu* menu, const Vector<WebContextMenuItemData>& items)
{
    for (const auto& data : items) {
        if (data.type() == WebCore::ContextMenuItemType::Separator) {
             menu->AddSeparatorItem();
        } else if (data.type() == WebCore::ContextMenuItemType::Submenu) {
             BMenu* submenu = new BMenu(data.title().utf8().data());
             populateMenuFromData(submenu, data.submenu());
             menu->AddItem(submenu);
        } else {
             BMessage* msg = new BMessage('cmic');
             msg->AddPointer("itemData", &data);
             BMenuItem* menuItem = new BMenuItem(data.title().utf8().data(), msg);
             menuItem->SetEnabled(data.enabled());
             menuItem->SetMarked(data.checked());
             menu->AddItem(menuItem);
        }
    }
}

void WebContextMenuProxyHaiku::showContextMenuWithItems(Vector<Ref<WebContextMenuItem>>&& items)
{
    if (items.isEmpty())
        return;

    if (m_menu)
        delete m_menu;

    m_menu = new BPopUpMenu("ContextMenu", false, false);

    // Build menu
    for (const auto& item : items) {
        const WebContextMenuItemData& data = item->data();
        if (data.type() == WebCore::ContextMenuItemType::Separator) {
            m_menu->AddSeparatorItem();
        } else if (data.type() == WebCore::ContextMenuItemType::Submenu) {
            BMenu* submenu = new BMenu(data.title().utf8().data());
            populateMenuFromData(submenu, data.submenu());
            m_menu->AddItem(submenu);
        } else {
             BMessage* msg = new BMessage('cmic');
             msg->AddPointer("itemData", &data);
             BMenuItem* menuItem = new BMenuItem(data.title().utf8().data(), msg);
             menuItem->SetEnabled(data.enabled());
             menuItem->SetMarked(data.checked());
             m_menu->AddItem(menuItem);
        }
    }

    WebCore::IntPoint location = menuLocation();
    BPoint screenPoint(location.x(), location.y());

    if (page() && page()->pageClient()) {
        WebCore::IntPoint screenLoc = page()->pageClient()->rootViewToScreen(location);
        screenPoint.Set(screenLoc.x(), screenLoc.y());
    }

    BMenuItem* selectedItem = m_menu->Go(screenPoint, false, true);

    if (selectedItem) {
         BMessage* msg = selectedItem->Message();
         WebContextMenuItemData* data = nullptr;
         if (msg && msg->FindPointer("itemData", (void**)&data) == B_OK && data) {
             page()->contextMenuItemSelected(*data, frameInfo());
         }
    }

    // We don't delete m_menu here immediately because it might be needed?
    // No, BPopUpMenu created with new needs deletion.
    // But `WebContextMenuProxyHaiku` destructor deletes it.
    // However, `showContextMenuWithItems` is likely the last thing called on this proxy for this showing.
    // If we delete it now, `m_menu` becomes invalid.

    delete m_menu;
    m_menu = nullptr;
}

} // namespace WebKit

#endif // ENABLE(CONTEXT_MENUS)
