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

#pragma once

#include "APIUIClient.h"
#include <wtf/Ref.h>

namespace WebKit {
class WebViewBase;
}

namespace WebKit {

class PageUIClientHaiku : public API::UIClient {
public:
    explicit PageUIClientHaiku(WebViewBase&);
    virtual ~PageUIClientHaiku();

    void printFrame(WebPageProxy&, WebFrameProxy&, const WebCore::FloatSize& pdfFirstPageSize, CompletionHandler<void()>&&) override;

    void createNewPage(WebPageProxy&, Ref<API::PageConfiguration>&&, Ref<API::NavigationAction>&&, CompletionHandler<void(RefPtr<WebPageProxy>&&)>&&) override;
    void showPage(WebPageProxy*) override;
    void close(WebPageProxy*) override;

    void runJavaScriptAlert(WebPageProxy&, const WTF::String&, WebFrameProxy*, FrameInfoData&&, Function<void()>&&) override;
    void runJavaScriptConfirm(WebPageProxy&, const WTF::String&, WebFrameProxy*, FrameInfoData&&, Function<void(bool)>&&) override;
    void runJavaScriptPrompt(WebPageProxy&, const WTF::String&, const WTF::String&, WebFrameProxy*, FrameInfoData&&, Function<void(const WTF::String&)>&&) override;

    void setStatusText(WebPageProxy*, const WTF::String&) override;
    void mouseDidMoveOverElement(WebPageProxy&, const WebHitTestResultData&, OptionSet<WebEventModifier>) override;

    void toolbarsAreVisible(WebPageProxy&, Function<void(bool)>&&) override;
    void setToolbarsAreVisible(WebPageProxy&, bool) override;
    void menuBarIsVisible(WebPageProxy&, Function<void(bool)>&&) override;
    void setMenuBarIsVisible(WebPageProxy&, bool) override;
    void statusBarIsVisible(WebPageProxy&, Function<void(bool)>&&) override;
    void setStatusBarIsVisible(WebPageProxy&, bool) override;
    void setIsResizable(WebPageProxy&, bool) override;

    void setWindowFrame(WebPageProxy&, const WebCore::FloatRect&) override;
    void windowFrame(WebPageProxy&, Function<void(WebCore::FloatRect)>&&) override;

private:
    WebViewBase& m_webView;
};

} // namespace WebKit
