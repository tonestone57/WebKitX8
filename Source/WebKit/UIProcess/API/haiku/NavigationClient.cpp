/*
 * Copyright (C) 2019, 2024 Haiku, Inc. All rights reserved.
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
#include "NavigationClient.h"

#include "APINavigation.h"
#include "APIObject.h"
#include "WebPageProxy.h"
#include "WebView.h"
#include "WebViewConstants.h"

#include <WebCore/LayoutMilestone.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>

#include <Looper.h>
#include <Message.h>
#include <String.h>

using namespace WebKit;

void NavigationClient::didStartProvisionalNavigation(WebPageProxy& page, const WebCore::ResourceRequest& request, API::Navigation* navigation, API::Object* userData)
{
    BMessage message(DID_START_PROVISIONAL_NAVIGATION);
    message.AddString("url", request.url().string().utf8().data());
    m_webView->getAppLooper()->PostMessage(&message);
}

void NavigationClient::didCommitNavigation(WebPageProxy& page, API::Navigation* navigation, API::Object* userData)
{
    BMessage message(DID_COMMIT_NAVIGATION);
    m_webView->getAppLooper()->PostMessage(&message);
}

void NavigationClient::didReceiveServerRedirectForProvisionalNavigation(WebPageProxy& page, API::Navigation* navigation, API::Object* userData)
{
    BMessage message(URL_CHANGE);
    message.AddString("url", BString(m_webView->getCurrentURL()));
    m_webView->getAppLooper()->PostMessage(&message);
}

void NavigationClient::didFinishNavigation(WebPageProxy& page, API::Navigation* navigation, API::Object* userData)
{
    BMessage message(DID_FINISH_NAVIGATION);
    m_webView->getAppLooper()->PostMessage(&message);
}

void NavigationClient::didFailProvisionalNavigationWithError(WebPageProxy& page, FrameInfoData&& frameInfo, API::Navigation* navigation, const WTF::URL& url, const WebCore::ResourceError& error, API::Object* userData)
{
    BMessage message(DID_FAIL_PROVISIONAL_NAVIGATION);
    message.AddString("url", url.string().utf8().data());
    message.AddString("error", error.localizedDescription().utf8().data());
    m_webView->getAppLooper()->PostMessage(&message);
}

void NavigationClient::didFailNavigationWithError(WebPageProxy& page, const FrameInfoData& frameInfo, API::Navigation* navigation, const WTF::URL& url, const WebCore::ResourceError& error, API::Object* userData)
{
    BMessage message(DID_FAIL_NAVIGATION);
    message.AddString("url", url.string().utf8().data());
    message.AddString("error", error.localizedDescription().utf8().data());
    m_webView->getAppLooper()->PostMessage(&message);
}

void NavigationClient::didSameDocumentNavigation(WebPageProxy& page, API::Navigation* navigation, SameDocumentNavigationType type, API::Object* userData)
{
    BMessage message(DID_SAME_DOCUMENT_NAVIGATION);
    message.AddInt32("type", static_cast<int32>(type));
    m_webView->getAppLooper()->PostMessage(&message);
}

void NavigationClient::renderingProgressDidChange(WebPageProxy& page, OptionSet<WebCore::LayoutMilestone> milestones)
{
    BMessage message(RENDERING_PROGRESS_DID_CHANGE);
    message.AddUInt32("milestones", milestones.toRaw());
    m_webView->getAppLooper()->PostMessage(&message);
}
