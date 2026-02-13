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
#include "WebViewConstants.h"

#include "WebCore/Region.h"

#include <View.h>
#include <Window.h>

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
        fWebView.ScrollTo(scrollPosition);
        fWebView.UnlockLooper();
    }
}

WebCore::FloatPoint PageClientImpl::viewScrollPosition()
{
    BPoint position;
    if (fWebView.LockLooper()) {
        position = fWebView.LeftTop();
        fWebView.UnlockLooper();
    }
    return position;
}

WebCore::IntSize PageClientImpl::viewSize()
{
    fWebView.Window()->Lock();
    BRect rect = fWebView.Frame();
    fWebView.Window()->Unlock();
    return IntSize(rect.right - rect.left, rect.bottom - rect.top);
}

bool PageClientImpl::isViewWindowActive()
{
    if (fWebView.Window())
        return fWebView.Window()->IsActive();
    return false;
}

bool PageClientImpl::isViewFocused()
{
    if (fWebView.Window())
        return fWebView.IsFocus();
    return false;
}

bool PageClientImpl::isActiveViewVisible()
{
    return !fWebView.IsHidden();
}

bool PageClientImpl::isViewInWindow()
{
    return fWebView.Window() != nullptr;
}

void PageClientImpl::processDidExit()
{
    if (BWindow* window = fWebView.Window())
        window->PostMessage(PROCESS_DID_EXIT);
}

void PageClientImpl::didRelaunchProcess()
{
    if (BWindow* window = fWebView.Window())
        window->PostMessage(PROCESS_DID_RELAUNCH);
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
        fWebView.ObscureCursor();
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
        rect = fWebView.ConvertToScreen(rect);
        fWebView.UnlockLooper();
        return rect;
    }
    return viewRect;
}

FloatRect PageClientImpl::convertToUserSpace(const FloatRect& viewRect)
{
    if (fWebView.LockLooper()) {
        BRect rect(viewRect);
        rect = fWebView.ConvertFromScreen(rect);
        fWebView.UnlockLooper();
        return rect;
    }
    return viewRect;
}

IntPoint PageClientImpl::screenToRootView(const IntPoint& point)
{
    if (fWebView.LockLooper()) {
        BPoint p(point);
        p = fWebView.ConvertFromScreen(p);
        fWebView.UnlockLooper();
        return IntPoint(p);
    }
    return point;
}

IntRect PageClientImpl::rootViewToScreen(const IntRect& rect)
{
    if (fWebView.LockLooper()) {
        BRect r(rect);
        r = fWebView.ConvertToScreen(r);
        fWebView.UnlockLooper();
        return IntRect(r);
    }
    return rect;
}

IntPoint PageClientImpl::rootViewToScreen(const IntPoint& point)
{
    if (fWebView.LockLooper()) {
        BPoint p(point);
        p = fWebView.ConvertToScreen(p);
        fWebView.UnlockLooper();
        return IntPoint(p);
    }
    return point;
}

void PageClientImpl::doneWithKeyEvent(const NativeWebKeyboardEvent& event, bool wasEventHandled)
{
    if (!wasEventHandled) {
        // If not handled by WebKit, we might want to let the system handle it.
        // For example, system shortcuts.
        // However, in BView model, events are consumed by the view mostly.
        // If we wanted to propagate, we would need to pass it up.
    }
}

RefPtr<WebPopupMenuProxy> PageClientImpl::createPopupMenuProxy(WebPageProxy& page)
{
    return WebPopupMenuProxyHaiku::create(fWebView, page);
}

#if ENABLE(CONTEXT_MENUS)
Ref<WebContextMenuProxy> PageClientImpl::createContextMenuProxy(WebPageProxy& page, FrameInfoData&& frameInfo, ContextMenuContextData&& context, const UserData& userData)
{
    return WebContextMenuProxyHaiku::create(fWebView, page, WTF::move(frameInfo), WTF::move(context), userData);
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
    // Handled by CoordinatedGraphics
}

void PageClientImpl::exitAcceleratedCompositingMode()
{
    // Handled by CoordinatedGraphics
}

void PageClientImpl::updateAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    // Handled by CoordinatedGraphics
}

void PageClientImpl::pageClosed()
{
    if (BWindow* window = fWebView.Window())
        window->PostMessage(PAGE_CLOSED);
}

void PageClientImpl::preferencesDidChange()
{
    // Force a redraw
    if (fWebView.LockLooper()) {
        fWebView.Invalidate();
        fWebView.UnlockLooper();
    }
}

void PageClientImpl::didChangeContentSize(const IntSize& size)
{
    if (BWindow* window = fWebView.Window()) {
        BMessage message(CONTENT_SIZE_CHANGED);
        message.AddFloat("width", size.width());
        message.AddFloat("height", size.height());
        window->PostMessage(&message);
    }
}

void PageClientImpl::didCommitLoadForMainFrame(const String& /* mimeType */, bool /* useCustomContentProvider */ )
{
}

void PageClientImpl::wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent& event)
{
    // The event was not handled by WebCore.
    // In many cases we might want to let the parent view handle it,
    // but simply ignoring it is also a valid strategy if we don't have a specific parent protocol.
}

void PageClientImpl::didFinishLoadingDataForCustomContentProvider(const String&, std::span<const unsigned char>)
{
}

void PageClientImpl::navigationGestureDidBegin()
{
    // Not implemented on Haiku
}

void PageClientImpl::navigationGestureWillEnd(bool, WebBackForwardListItem&)
{
    // Not implemented on Haiku
}

void PageClientImpl::navigationGestureDidEnd(bool, WebBackForwardListItem&)
{
    // Not implemented on Haiku
}

void PageClientImpl::navigationGestureDidEnd()
{
    // Not implemented on Haiku
}

void PageClientImpl::willRecordNavigationSnapshot(WebBackForwardListItem&)
{
    // Not implemented on Haiku
}

void PageClientImpl::didRemoveNavigationGestureSnapshot()
{
    // Not implemented on Haiku
}

void PageClientImpl::didFirstVisuallyNonEmptyLayoutForMainFrame()
{
}

void PageClientImpl::printFrame(WebFrameProxy&)
{
    if (fWebView.LockLooper()) {
        BPrintJob printJob("WebKit Page");
        if (printJob.ConfigJob() == B_OK) {
            printJob.BeginJob();
            BRect printableRect = printJob.PrintableRect();
            int32 firstPage = printJob.FirstPage();
            int32 lastPage = printJob.LastPage();

            // This is a simplified implementation that prints the current view content.
            // Ideally, we should ask WebCore to layout for printing.

            for (int32 page = firstPage; page <= lastPage; ++page) {
                printJob.DrawView(&fWebView, printableRect, BPoint(0, 0));
                printJob.SpoolPage();
            }
            printJob.CommitJob();
        }
        fWebView.UnlockLooper();
    }
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
    if (auto* page = fWebView.page()) {
        if (std::optional<WebCore::Color> color = page->backgroundColor()) {
             auto srgba = color->toColorTypeLossy<SRGBA<uint8_t>>();
             if (fWebView.LockLooper()) {
                 rgb_color haikuColor = {
                     srgba.red,
                     srgba.green,
                     srgba.blue,
                     srgba.alpha
                 };
                 fWebView.SetViewColor(haikuColor);
                 fWebView.Invalidate();
                 fWebView.UnlockLooper();
             }
        }
    }
}

void PageClientImpl::isPlayingAudioWillChange()
{
}

void PageClientImpl::isPlayingAudioDidChange()
{
    if (BWindow* window = fWebView.Window()) {
        BMessage message(IS_PLAYING_AUDIO_CHANGED);
        bool isPlaying = false;
        if (auto* page = fWebView.page())
            isPlaying = page->isPlayingAudio();
        message.AddBool("playing", isPlaying);
        window->PostMessage(&message);
    }
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
