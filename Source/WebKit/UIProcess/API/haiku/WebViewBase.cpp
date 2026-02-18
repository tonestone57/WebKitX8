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
#include "WebViewBase.h"

#include "APIPageConfiguration.h"
#include "CertificateInfoDialog.h"
#include "DrawingAreaProxy.h"

#if ENABLE(REMOTE_INSPECTOR)
#include "RemoteInspectorProtocolHandler.h"
#endif

#include "NativeWebMouseEvent.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebWheelEvent.h"
#include "NativeWebTouchEvent.h"

#include "PageClientImplHaiku.h"
#include "PageUIClientHaiku.h"
#include "PageLoadState.h"
#include "WebPageGroup.h"
#include "WebProcessPool.h"
#include "WebCore/Cursor.h"
#include "WebCore/IntRect.h"
#include "WebCore/Region.h"
#include "WebViewConstants.h"
#include "wtf/MainThread.h"

#include <Application.h>
#include <Cursor.h>
#include <Window.h>
#include <ctime>

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
#include "DrawingAreaProxyCoordinatedGraphics.h"
#endif

using namespace WebKit;
using namespace WebCore;

WebViewBase::WebViewBase(const char* name, BRect rect, BWindow* parentWindow,
    const API::PageConfiguration& pageConfig)
    : BView(name, B_WILL_DRAW | B_FRAME_EVENTS | B_FULL_UPDATE_ON_RESIZE | B_NAVIGABLE)
    , fPageClient(makeUniqueWithoutRefCountedCheck<PageClientImpl>(*this))
{
    auto config = pageConfig.copy();

    WebProcessPool& processPool = config->processPool();
    fPage = processPool.createWebPage(*fPageClient, std::move(config));
    fPage->setUIClient(makeUnique<PageUIClientHaiku>(*this));

#if ENABLE(REMOTE_INSPECTOR)
    fPage->setURLSchemeHandlerForScheme(RemoteInspectorProtocolHandler::create(*fPage), "inspector"_s);
#endif

    fPage->initializeWebPage(Site(aboutBlankURL()), {}, {});

    if (fPage->drawingArea()) {
        fPage->drawingArea()->setSize(IntSize(rect.IntegerWidth() + 1, rect.IntegerHeight() + 1));
    }
}

const char* WebViewBase::currentURL() const
{
    return page()->pageLoadState().activeURL().utf8().data();
}

void WebViewBase::FrameResized(float newWidth, float newHeight)
{
#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    auto drawingArea = static_cast<DrawingAreaProxyCoordinatedGraphics*>(page()->drawingArea());
    if (!drawingArea)
        return;
    drawingArea->setSize(IntSize(newWidth + 1, newHeight + 1));
#endif
}

void WebViewBase::MessageReceived(BMessage* message)
{
    switch (message->what)
    {
        case SHOW_CERTIFICATE_INFO: {
            const auto& certificateInfo = fPage->pageLoadState().certificateInfo();
            auto summary = certificateInfo.summary();

            if (!summary)
                break;

            auto formatDate = [](Seconds seconds) {
                time_t t = static_cast<time_t>(seconds.seconds());
                struct tm tm;
                localtime_r(&t, &tm);
                char buffer[64];
                strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
                return String::fromUTF8(buffer);
            };

            CertificateInfoDialog::show(
                currentURL(),
                summary->issuer.utf8().data(),
                summary->subject.utf8().data(),
                formatDate(summary->validFrom).utf8().data(),
                formatDate(summary->validUntil).utf8().data(),
                summary->fingerprint.utf8().data()
            );
            break;
        }
        case B_MOUSE_WHEEL_CHANGED:
            callOnMainRunLoop([weakThis = WeakPtr { *this }, message = *message](){
                if (weakThis)
                    weakThis->fPage->handleWheelEvent(NativeWebWheelEvent(&message));
            });
            break;
        case B_MOUSE_DOWN:
            MakeFocus(true);
            // fallthrough
        case B_MOUSE_UP:
        case B_MOUSE_MOVED:
            callOnMainRunLoop([weakThis = WeakPtr { *this }, message = *message](){
                if (weakThis)
                    weakThis->fPage->handleMouseEvent(NativeWebMouseEvent(&message));
            });
            break;
        case B_KEY_DOWN:
        case B_KEY_UP:
        case B_UNMAPPED_KEY_DOWN:
        case B_UNMAPPED_KEY_UP:
            callOnMainRunLoop([weakThis = WeakPtr { *this }, message = *message](){
                if (weakThis)
                    weakThis->fPage->handleKeyboardEvent(NativeWebKeyboardEvent(&message));
            });
            break;
        case B_TOUCH_DOWN:
        case B_TOUCH_UP:
        case B_TOUCH_MOVED:
        case B_TOUCH_CANCEL:
            callOnMainRunLoop([weakThis = WeakPtr { *this }, message = *message](){
                if (weakThis)
                    weakThis->fPage->handleTouchEvent(nullptr, NativeWebTouchEvent(&message));
            });
            break;
        default:
            BView::MessageReceived(message);
            break;
    }
}

void WebViewBase::MakeFocus(bool focused)
{
    BView::MakeFocus(focused);
    fPage->setFocus(focused);
}

void WebViewBase::Draw(BRect update)
{
#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    auto drawingArea = static_cast<DrawingAreaProxyCoordinatedGraphics*>(page()->drawingArea());
    if (!drawingArea)
        return;

    IntRect updateArea(update);
    WebCore::Region unpainted;
    drawingArea->paint(this, updateArea, unpainted);
#endif
}

void WebViewBase::paint(const IntRect& dirtyRect)
{
    // The DrawingAreaProxy handles painting updates, usually triggering Draw().
    // If this is called explicitly (e.g. for testing or forced update),
    // we should invalidate the region.
    if (LockLooper()) {
        Invalidate(BRect(dirtyRect));
        UnlockLooper();
    }
}

void WebViewBase::setCursor(const WebCore::Cursor& cursor)
{
    if (LockLooper()) {
        if (cursor.platformCursor())
            SetViewCursor(cursor.platformCursor());
        else
            SetViewCursor(B_CURSOR_SYSTEM_DEFAULT);
        UnlockLooper();
    }
}

void WebViewBase::setToolTip(const char* toolTip)
{
    if (LockLooper()) {
        SetToolTip(toolTip);
        UnlockLooper();
    }
}
