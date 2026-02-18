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

#include <WebCore/DragItem.h>
#include "DrawingAreaProxy.h"
#include "WebProcessProxy.h"
#include "WebColorPicker.h"
#include "WebDataListSuggestionsDropdown.h"
#include "WebViewBase.h"
#include "../../haiku/WebColorPickerHaiku.h"
#include "../../haiku/WebContextMenuProxyHaiku.h"
#include "../../haiku/WebDateTimePickerHaiku.h"
#include "../../haiku/WebPopupMenuProxyHaiku.h"
#include "../../haiku/WebContextMenuProxyHaiku.h"
#include "WebViewConstants.h"

#include "WebCore/Region.h"
#include "WebFrameProxy.h"
#include "ShareableBitmap.h"
#include "WebCore/ShareableBitmap.h"
#include "WebFullScreenManagerProxy.h"
#include "PrintInfo.h"

#include <View.h>
#include <Window.h>
#include <PrintJob.h>
#include <Clipboard.h>
#include <Bitmap.h>

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
#include "DrawingAreaProxyCoordinatedGraphics.h"
#endif

namespace WebKit {

using namespace WebCore;

#if ENABLE(FULLSCREEN_API)
class WebFullScreenManagerProxyClientHaiku final : public WebFullScreenManagerProxyClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebFullScreenManagerProxyClientHaiku(WebViewBase& view) : m_view(view) { }

    void closeFullScreenManager() override { }
    bool isFullScreen() override {
        if (auto* window = m_view.Window())
            return window->IsFullScreen();
        return false;
    }
    void enterFullScreen(WebCore::FloatSize, CompletionHandler<void(bool)>&& completionHandler) override {
        if (auto* window = m_view.Window()) {
            if (window->Lock()) {
                window->SetFullScreen(true);
                window->Unlock();
                completionHandler(true);
                return;
            }
        }
        completionHandler(false);
    }
    void exitFullScreen(CompletionHandler<void()>&& completionHandler) override {
        if (auto* window = m_view.Window()) {
            if (window->Lock()) {
                window->SetFullScreen(false);
                window->Unlock();
            }
        }
        completionHandler();
    }
    void beganEnterFullScreen(const WebCore::IntRect&, const WebCore::IntRect&, CompletionHandler<void(bool)>&& completionHandler) override { completionHandler(true); }
    void beganExitFullScreen(const WebCore::IntRect&, const WebCore::IntRect&, CompletionHandler<void()>&& completionHandler) override { completionHandler(); }

private:
    WebViewBase& m_view;
};
#endif

PageClientImpl::PageClientImpl(WebViewBase& view)
    : fWebView(view)
{
#if ENABLE(FULLSCREEN_API)
    m_fullScreenManagerProxyClient = makeUnique<WebFullScreenManagerProxyClientHaiku>(view);
#endif
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
    if (fWebView.LockLooper()) {
        BRect bounds = fWebView.Bounds();
        fWebView.UnlockLooper();
        return IntSize(bounds.IntegerWidth() + 1, bounds.IntegerHeight() + 1);
    }
    return IntSize();
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
    if (fWebView.LockLooper()) {
        fWebView.SetViewCursor(cursor.platformCursor());
        fWebView.UnlockLooper();
    }
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

void PageClientImpl::didCommitLoadForMainFrame(const String&, bool)
{
    if (BWindow* window = fWebView.Window())
        window->PostMessage(LOAD_COMMITTED);
}

void PageClientImpl::wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent& event)
{
    // If the wheel event wasn't handled, we might want to propagate it to the parent view.
    // However, BView event handling is usually done via MessageReceived.
    // If we want standard BView scrolling behavior when web content doesn't scroll, we might need to invoke it here.
    // For now, let's leave it as a no-op or maybe beep?
}

void PageClientImpl::didFinishLoadingDataForCustomContentProvider(const String&, std::span<const unsigned char>)
{
    // If we implement custom content providers (e.g. PDF viewer), this would be used.
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
    if (BWindow* window = fWebView.Window())
        window->PostMessage(DID_FIRST_VISUALLY_NON_EMPTY_LAYOUT);
}

class AsyncPrinter : public RefCounted<AsyncPrinter> {
public:
    static void Print(WebViewBase& view, WebPageProxy& page, WebFrameProxy& frame)
    {
        adoptRef(*new AsyncPrinter(view, page, frame))->start();
    }

private:
    AsyncPrinter(WebViewBase& view, WebPageProxy& page, WebFrameProxy& frame)
        : m_view(view)
        , m_page(page)
        , m_frame(frame)
    {
    }

    void start()
    {
        if (!m_view || !m_view->LockLooper())
            return;

        m_printJob = std::make_shared<BPrintJob>("WebKit Page");
        if (m_printJob->ConfigJob() != B_OK) {
            m_view->UnlockLooper();
            return;
        }

        BRect printableRect = m_printJob->PrintableRect();
        m_view->UnlockLooper();

        PrintInfo printInfo;
        printInfo.pageSetupScaleFactor = 1.0;
        printInfo.availablePaperWidth = printableRect.Width();
        printInfo.availablePaperHeight = printableRect.Height();
        printInfo.rect = IntRect(0, 0, printableRect.Width(), printableRect.Height());

        m_page->computePagesForPrinting(m_frame, printInfo, [this, protectedThis = Ref { *this }](const Vector<IntRect>& pageRects, double totalScaleFactor, const WebCore::FloatBoxExtent&) {
            m_pageRects = pageRects;
            m_scaleFactor = totalScaleFactor;
            snapshotNextPage();
        });
    }

    void snapshotNextPage()
    {
        if (m_pageIndex >= m_pageRects.size()) {
            print();
            return;
        }

        // Optimize: Only snapshot pages within the requested range
        int32 firstPage = m_printJob->FirstPage();
        int32 lastPage = m_printJob->LastPage();
        int32 currentPage = m_pageIndex + 1; // 1-based index

        if (currentPage < firstPage || currentPage > lastPage) {
            m_snapshots.append(nullptr);
            m_pageIndex++;
            snapshotNextPage();
            return;
        }

        WebCore::IntRect rect = m_pageRects[m_pageIndex];

        WebCore::IntSize snapshotSize = rect.size();
        if (m_scaleFactor != 1.0 && m_scaleFactor > 0) {
            snapshotSize.scale(m_scaleFactor);
        }

        BRect printableRect = m_printJob->PrintableRect();
        PrintInfo printInfo;
        printInfo.pageSetupScaleFactor = 1.0;
        printInfo.availablePaperWidth = printableRect.Width();
        printInfo.availablePaperHeight = printableRect.Height();
        printInfo.rect = IntRect(0, 0, (int)printableRect.Width(), (int)printableRect.Height());

        m_page->drawRectToImage(m_frame, printInfo, rect, snapshotSize, [this, protectedThis = Ref { *this }](std::optional<ShareableBitmap::Handle>&& imageHandle) {
            if (imageHandle) {
                m_snapshots.append(ShareableBitmap::create(WTFMove(*imageHandle)));
            } else {
                m_snapshots.append(nullptr);
            }
            m_pageIndex++;
            snapshotNextPage();
        });
    }

    class PrintView : public BView {
    public:
        PrintView(BRect frame, BBitmap* bitmap)
            : BView(frame, "print", 0, B_WILL_DRAW)
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

    void print()
    {
        if (!m_view || !m_view->LockLooper())
            return;

        m_printJob->BeginJob();

        int32 firstPage = m_printJob->FirstPage();
        int32 lastPage = m_printJob->LastPage();

        // Clamp to available pages
        if (lastPage > (int32)m_snapshots.size())
            lastPage = m_snapshots.size();

        BRect printableRect = m_printJob->PrintableRect();

        for (int32 i = firstPage - 1; i < lastPage; ++i) {
            if (i >= 0 && i < (int32)m_snapshots.size() && m_snapshots[i]) {
                if (auto bitmap = m_snapshots[i]->createBBitmap()) {
                    if (BWindow* window = m_view->Window()) {
                        PrintView* printView = new PrintView(printableRect, bitmap.get());
                        printView->Hide();
                        window->AddChild(printView);

                        m_printJob->DrawView(printView, printableRect, BPoint(0, 0));

                        window->RemoveChild(printView);
                        delete printView;
                    }
                }
            }
            m_printJob->SpoolPage();
        }
        m_printJob->CommitJob();
        m_view->UnlockLooper();
    }

    WeakPtr<WebViewBase> m_view;
    Ref<WebPageProxy> m_page;
    Ref<WebFrameProxy> m_frame;
    std::shared_ptr<BPrintJob> m_printJob;
    Vector<IntRect> m_pageRects;
    double m_scaleFactor { 1.0 };
    Vector<RefPtr<ShareableBitmap>> m_snapshots;
    size_t m_pageIndex { 0 };
};

void PageClientImpl::printFrame(WebFrameProxy& frame)
{
    AsyncPrinter::Print(fWebView, *fWebView.page(), frame);
}

void PageClientImpl::didFinishNavigation(API::Navigation*)
{
    if (BWindow* window = fWebView.Window())
        window->PostMessage(DID_FINISH_NAVIGATION);
}

void PageClientImpl::didFailNavigation(API::Navigation*)
{
    if (BWindow* window = fWebView.Window())
        window->PostMessage(DID_FAIL_NAVIGATION);
}

void PageClientImpl::didSameDocumentNavigationForMainFrame(SameDocumentNavigationType)
{
    if (BWindow* window = fWebView.Window())
        window->PostMessage(DID_SAME_DOCUMENT_NAVIGATION);
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
    // The view is owned by the BWindow hierarchy, but PageClient might be refcounted.
    // However, PageClientImpl is owned by WebViewBase uniquely.
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
    return *m_fullScreenManagerProxyClient;
}
#endif

void PageClientImpl::startDrag(const WebCore::DragItem& dragItem, WebCore::ShareableBitmap::Handle&& dragImageHandle, const std::optional<WebCore::NodeIdentifier>&)
{
    if (!fWebView.LockLooper())
        return;

    BMessage dragMessage;

    // Attempt to read from the "WebKitDrag" clipboard where PasteboardHaiku wrote the data
    BClipboard clipboard("WebKitDrag");
    if (clipboard.Lock()) {
        if (BMessage* data = clipboard.Data()) {
            dragMessage = *data;
        }
        clipboard.Unlock();
    }

    BBitmap* dragBitmap = nullptr;
    auto shareableBitmap = ShareableBitmap::create(WTFMove(dragImageHandle));
    if (shareableBitmap) {
        auto bitmap = shareableBitmap->createBBitmap();
        if (bitmap)
            dragBitmap = bitmap.release();
    }

    // Use the anchor point from DragItem to position the image correctly relative to the cursor
    BPoint offset(-dragItem.imageAnchorPoint.x(), -dragItem.imageAnchorPoint.y());

    fWebView.DragMessage(&dragMessage, dragBitmap, B_OP_ALPHA, offset);

    // The bitmap is owned by the drag message once DragMessage is called.
    // It will be deleted by the system when the drag is finished.

    fWebView.UnlockLooper();
}

}
