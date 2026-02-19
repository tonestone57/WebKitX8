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

#pragma once

#if ENABLE(CONTEXT_MENUS)

#include "WebContextMenuProxy.h"
#include "WebPageProxy.h"
#include "WebViewBase.h"
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

class BPopUpMenu;
class BMenu;

namespace WebKit {

class WebContextMenuProxyHaiku : public WebContextMenuProxy {
public:
    static Ref<WebContextMenuProxyHaiku> create(WebViewBase& webView, WebPageProxy& page, FrameInfoData&& frameInfo, ContextMenuContextData&& context, const UserData& userData)
    {
        return adoptRef(*new WebContextMenuProxyHaiku(webView, page, WTF::move(frameInfo), WTF::move(context), userData));
    }

    ~WebContextMenuProxyHaiku();

    void showContextMenuWithItems(Vector<Ref<WebContextMenuItem>>&&) override;

private:
    WebContextMenuProxyHaiku(WebViewBase&, WebPageProxy&, FrameInfoData&&, ContextMenuContextData&&, const UserData&);

    void populateMenu(BMenu* menu, const Vector<WebContextMenuItemData>& items);

    WebViewBase& m_webView;
    WeakPtr<WebViewBase> m_weakWebView;
    WeakPtr<WebPageProxy> m_weakPage;
    BPopUpMenu* m_menu;
    Vector<WebContextMenuItemData> m_currentItems;
};

} // namespace WebKit

#endif // ENABLE(CONTEXT_MENUS)
