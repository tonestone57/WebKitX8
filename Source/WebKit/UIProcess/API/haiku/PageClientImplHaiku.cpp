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

#include "WebCore/BitmapImage.h"
#include "WebCore/Image.h"
#include "WebCore/NativeImage.h"
#include "WebCore/Region.h"
#include "WebCore/ShareableBitmap.h"
#include "PrintInfo.h"

#include <Bitmap.h>
#include <PrintJob.h>
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

void PageClientImpl::PageClientImpl::processDidExit()
{
    // fprintf(stderr, "PageClientImpl::processDidExit\n");
}

void PageClientImpl::didRelaunchProcess()
{
    // fprintf(stderr, "PageClientImpl::didRelaunchProcess\n");
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
    // If the event wasn't handled by WebKit, we might want to pass it to the BView's default handling?
    // But usually BView::KeyDown is what triggered this, so we are done.
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
    // Cleanup if needed
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
    // Should update scrollbars?
    // fWebView.SetContentSize(size); // Assuming BView or similar has this concept or we manage scrollbars
}

void PageClientImpl::didCommitLoadForMainFrame(const String& /* mimeType */, bool /* useCustomContentProvider */ )
{
}

void PageClientImpl::wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent& event)
{
    // Pass back to BView?
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

class BitmapPrintView : public BView {
public:
    BitmapPrintView(BRect frame, BBitmap* bitmap)
        : BView(frame, "print_view", B_FOLLOW_NONE, B_WILL_DRAW)
        , m_bitmap(bitmap)
    {
    }

    void Draw(BRect) override
    {
        if (m_bitmap)
            DrawBitmap(m_bitmap, Bounds());
    }

private:
    BBitmap* m_bitmap;
};

void PageClientImpl::printFrame(WebFrameProxy& frame)
{
    // Printing is a blocking operation on Haiku's BPrintJob, but WebKit requires async page computation.
    // We start the config here, then trigger the async process.
    if (!fWebView.LockLooper())
        return;

    auto printJob = std::make_shared<BPrintJob>("WebKit Page");
    if (printJob->ConfigJob() != B_OK) {
        fWebView.UnlockLooper();
        return;
    }

    BRect paper = printJob->PaperRect();
    BRect printable = printJob->PrintableRect();
    fWebView.UnlockLooper();

    float marginLeft = printable.left - paper.left;
    float marginTop = printable.top - paper.top;
    float marginRight = paper.right - printable.right;
    float marginBottom = paper.bottom - printable.bottom;

    // Convert BRect to PrintInfo. Note: BRect is pixel based, PrintInfo expects points?
    // Haiku BPrintJob rects are usually 72dpi points.
    WebKit::PrintInfo printInfo(1.0f, printable.Width(), printable.Height(),
        WebCore::FloatBoxExtent(marginTop, marginRight, marginBottom, marginLeft));

    auto page = fWebView.page();
    if (!page)
        return;

    page->beginPrinting(&frame, printInfo);

    page->computePagesForPrinting(frame.frameID(), printInfo,
        [printJob, page, frameID = frame.frameID()](const Vector<WebCore::IntRect>& pageRects, double, const WebCore::FloatBoxExtent&) {
            auto snapshotQueue = std::make_shared<Vector<WebCore::IntRect>>(pageRects);
            auto bitmaps = std::make_shared<Vector<BBitmap*>>();

            // Recursive lambda to process snapshots sequentially
            auto chain = [printJob, page, frameID, snapshotQueue, bitmaps](auto&& self, size_t index) -> void {
                if (index >= snapshotQueue->size()) {
                    // All snapshots taken, perform actual printing
                    if (printJob->BeginJob()) {
                        int32 firstPage = printJob->FirstPage();
                        int32 lastPage = printJob->LastPage();

                        if (firstPage < 1) firstPage = 1;
                        if (lastPage > (int32)snapshotQueue->size()) lastPage = snapshotQueue->size();

                        BRect printableRect = printJob->PrintableRect();

                        for (int32 i = firstPage; i <= lastPage; ++i) {
                            BBitmap* bmp = (*bitmaps)[i - 1];
                            if (bmp) {
                                BitmapPrintView printView(printableRect, bmp);
                                printJob->DrawView(&printView, printableRect, BPoint(0, 0));
                                printJob->SpoolPage();
                            }
                        }
                        printJob->CommitJob();
                    }
                    page->endPrinting();
                    for (auto* b : *bitmaps)
                        delete b;
                    return;
                }

                WebCore::IntRect rect = (*snapshotQueue)[index];
                WebCore::IntSize size = rect.size();
                // Use SnapshotOptionsNone or similar. 0 is default.
                WebCore::SnapshotOptions options = 0;

                page->takeSnapshot(rect, size, options,
                    [printJob, page, frameID, snapshotQueue, bitmaps, index, self = std::forward<decltype(self)>(self)](std::optional<WebCore::ShareableBitmapHandle>&& handle) {
                        BBitmap* bmp = nullptr;
                        if (handle) {
                            if (auto sb = WebCore::ShareableBitmap::create(*handle)) {
                                if (auto img = sb->createImage()) {
                                    if (auto native = img->nativeImage()) {
                                        const BBitmap* platformImg = native->platformImage();
                                        if (platformImg)
                                            bmp = new BBitmap(platformImg);
                                    }
                                }
                            }
                        }
                        bitmaps->append(bmp);
                        self(self, index + 1);
                    });
            };

            chain(chain, 0);
        });
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
    // fWebView.SetViewColor(page->backgroundColor());
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
    // Could update window title or icon to show audio status
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
