/*
 * Copyright (C) 2019 Haiku, Inc. All rights reserved.
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
#include "PageClientImplHaiku.h"

#include "DrawingAreaProxy.h"
#include "WebProcessProxy.h"
#include "WebColorPicker.h"
#include "WebDataListSuggestionsDropdown.h"
#include "WebViewBase.h"
#include "../../haiku/WebColorPickerHaiku.h"
#include "../../haiku/WebContextMenuProxyHaiku.h"
#include "../../haiku/WebDateTimePickerHaiku.h"
#include "../../haiku/WebPopupMenuProxyHaiku.h"

#include "WebCore/Region.h"

#include <Application.h>
#include <View.h>
#include <Window.h>
#include <stdio.h>

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
#include "DrawingAreaProxyCoordinatedGraphics.h"
#endif

namespace WebKit {

using namespace WebCore;

PageClientImpl::PageClientImpl(WebViewBase& view)
    : fWebView(view)
{
}

WTF::Ref<DrawingAreaProxy> PageClientImpl::createDrawingAreaProxy(WebKit::WebProcessProxy& processProxy)
{
#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    return DrawingAreaProxyCoordinatedGraphics::create(*fWebView.page(), processProxy);
#else
    RELEASE_ASSERT_NOT_REACHED();
#endif
}

void PageClientImpl::setViewNeedsDisplay(const WebCore::Region& region)
{
    if (fWebView.LockLooper()) {
        fWebView.Invalidate(region.bounds());
        fWebView.UnlockLooper();
    }
}

void PageClientImpl::requestScroll(const WebCore::FloatPoint& scrollPosition, const WebCore::IntPoint&, WebCore::ScrollIsAnimated)
{
    if (fWebView.LockLooper()) {
        fWebView.ScrollTo(scrollPosition.x(), scrollPosition.y());
        fWebView.UnlockLooper();
    }
}

WebCore::FloatPoint PageClientImpl::viewScrollPosition()
{
    if (fWebView.LockLooper()) {
        BRect bounds = fWebView.Bounds();
        fWebView.UnlockLooper();
        return FloatPoint(bounds.left, bounds.top);
    }
    return { };
}

WebCore::IntSize PageClientImpl::viewSize()
{
    if (fWebView.LockLooper()) {
        BRect rect = fWebView.Bounds();
        fWebView.UnlockLooper();
        return IntSize(rect.IntegerWidth() + 1, rect.IntegerHeight() + 1);
    }
    return { };
}

bool PageClientImpl::isViewWindowActive()
{
    if (BWindow* window = fWebView.Window())
        return window->IsActive();
    return false;
}

bool PageClientImpl::isViewFocused()
{
    if (fWebView.LockLooper()) {
        bool focused = fWebView.IsFocus();
        fWebView.UnlockLooper();
        return focused;
    }
    return false;
}

bool PageClientImpl::isActiveViewVisible()
{
    if (fWebView.LockLooper()) {
        bool visible = !fWebView.IsHidden();
        fWebView.UnlockLooper();
        return visible;
    }
    return true;
}

bool PageClientImpl::isViewInWindow()
{
    return fWebView.Window() != nullptr;
}

void PageClientImpl::processDidExit()
{
    fprintf(stderr, "PageClientImpl::processDidExit\n");
}

void PageClientImpl::didRelaunchProcess()
{
    fprintf(stderr, "PageClientImpl::didRelaunchProcess\n");
}

void PageClientImpl::toolTipChanged(const String&, const String& newToolTip)
{
    fWebView.setToolTip(newToolTip.utf8().data());
}

void PageClientImpl::setCursor(const WebCore::Cursor& cursor)
{
    fWebView.setCursor(cursor);
}

void PageClientImpl::setCursorHiddenUntilMouseMoves(bool hiddenUntilMouseMoves)
{
    if (hiddenUntilMouseMoves)
        be_app->ObscureCursor();
}

void PageClientImpl::registerEditCommand(Ref<WebEditCommandProxy>&& command, UndoOrRedo undoOrRedo)
{
    fUndoController.registerEditCommand(std::move(command), undoOrRedo);
}

void PageClientImpl::clearAllEditCommands()
{
    fUndoController.clearAllEditCommands();
}

bool PageClientImpl::canUndoRedo(UndoOrRedo undoOrRedo)
{
    return fUndoController.canUndoRedo(undoOrRedo);
}

void PageClientImpl::executeUndoRedo(UndoOrRedo undoOrRedo)
{
    fUndoController.executeUndoRedo(undoOrRedo);
}

FloatRect PageClientImpl::convertToDeviceSpace(const FloatRect& viewRect)
{
    if (fWebView.LockLooper()) {
        BRect rect(viewRect);
        fWebView.ConvertToScreen(&rect);
        fWebView.UnlockLooper();
        return rect;
    }
    return viewRect;
}

FloatRect PageClientImpl::convertToUserSpace(const FloatRect& viewRect)
{
    if (fWebView.LockLooper()) {
        BRect rect(viewRect);
        fWebView.ConvertFromScreen(&rect);
        fWebView.UnlockLooper();
        return rect;
    }
    return viewRect;
}

IntPoint PageClientImpl::screenToRootView(const IntPoint& point)
{
    if (fWebView.LockLooper()) {
        BPoint p(point);
        fWebView.ConvertFromScreen(&p);
        fWebView.UnlockLooper();
        return IntPoint(p);
    }
    return point;
}

IntRect PageClientImpl::rootViewToScreen(const IntRect& rect)
{
    if (fWebView.LockLooper()) {
        BRect r(rect);
        fWebView.ConvertToScreen(&r);
        fWebView.UnlockLooper();
        return IntRect(r);
    }
    return rect;
}

IntPoint PageClientImpl::rootViewToScreen(const IntPoint& point)
{
    if (fWebView.LockLooper()) {
        BPoint p(point);
        fWebView.ConvertToScreen(&p);
        fWebView.UnlockLooper();
        return IntPoint(p);
    }
    return point;
}

void PageClientImpl::doneWithKeyEvent(const NativeWebKeyboardEvent& event, bool wasEventHandled)
{
}

RefPtr<WebPopupMenuProxy> PageClientImpl::createPopupMenuProxy(WebPageProxy& page)
{
    return WebPopupMenuProxyHaiku::create(fWebView, page);
}

#if ENABLE(CONTEXT_MENUS)
Ref<WebContextMenuProxy> PageClientImpl::createContextMenuProxy(WebPageProxy& page, FrameInfoData&& frameInfo, ContextMenuContextData&& context, const UserData& userData)
{
    return WebContextMenuProxyHaiku::create(page, WTF::move(frameInfo), WTF::move(context), userData);
}
#endif

RefPtr<WebColorPicker> PageClientImpl::createColorPicker(WebPageProxy& page, const WebCore::Color& initialColor,
    const WebCore::IntRect&, WebKit::ColorControlSupportsAlpha, Vector<WebCore::Color>&&)
{
    return WebColorPickerHaiku::create(page, initialColor);
}

WTF::RefPtr<WebKit::WebDataListSuggestionsDropdown> PageClientImpl::createDataListSuggestionsDropdown(WebKit::WebPageProxy&)
{
    return nullptr;
}

void PageClientImpl::enterAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
}

void PageClientImpl::exitAcceleratedCompositingMode()
{
}

void PageClientImpl::updateAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
}

void PageClientImpl::pageClosed()
{
}

void PageClientImpl::preferencesDidChange()
{
}

void PageClientImpl::didChangeContentSize(const IntSize& size)
{
}

void PageClientImpl::didCommitLoadForMainFrame(const String& /* mimeType */, bool /* useCustomContentProvider */ )
{
}

void PageClientImpl::wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent& event)
{
}

void PageClientImpl::didFinishLoadingDataForCustomContentProvider(const String&, std::span<const unsigned char>)
{
}

void PageClientImpl::navigationGestureDidBegin()
{
}

void PageClientImpl::navigationGestureWillEnd(bool, WebBackForwardListItem&)
{
}

void PageClientImpl::navigationGestureDidEnd(bool, WebBackForwardListItem&)
{
}

void PageClientImpl::navigationGestureDidEnd()
{
}

void PageClientImpl::willRecordNavigationSnapshot(WebBackForwardListItem&)
{
}

void PageClientImpl::didRemoveNavigationGestureSnapshot()
{
}

void PageClientImpl::didFirstVisuallyNonEmptyLayoutForMainFrame()
{
}

void PageClientImpl::didFinishNavigation(API::Navigation*)
{
}

void PageClientImpl::didFailNavigation(API::Navigation*)
{
}

void PageClientImpl::didSameDocumentNavigationForMainFrame(SameDocumentNavigationType)
{
}

void PageClientImpl::didChangeBackgroundColor()
{
    if (fWebView.LockLooper()) {
        fWebView.Invalidate();
        fWebView.UnlockLooper();
    }
}

void PageClientImpl::isPlayingAudioWillChange()
{
}

void PageClientImpl::isPlayingAudioDidChange()
{
}

void PageClientImpl::refView()
{
}

void PageClientImpl::derefView()
{
}

WebViewBase* PageClientImpl::viewWidget()
{
    return &fWebView;
}

RefPtr<WebDateTimePicker> PageClientImpl::createDateTimePicker(WebPageProxy& page)
{
    return WebDateTimePickerHaiku::create(page);
}

#if ENABLE(FULLSCREEN_API)
WebFullScreenManagerProxyClient& PageClientImpl::fullScreenManagerProxyClient()
{
    // FIXME: Implement full screen support
    RELEASE_ASSERT_NOT_REACHED();
    return *static_cast<WebFullScreenManagerProxyClient*>(nullptr);
}
#endif

}
