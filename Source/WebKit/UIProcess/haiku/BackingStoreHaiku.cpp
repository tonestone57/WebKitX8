/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Samsung Electronics
 * Copyright (C) 2011,2014 Igalia S.L.
 * Copyright (C) 2024 Sony Interactive Entertainment Inc.
 * Copyright (C) 2024 Haiku, Inc.
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
#include "BackingStore.h"

#if USE(COORDINATED_GRAPHICS)

#include "UpdateInfo.h"

#include <WebCore/GraphicsContext.h>
#include <WebCore/IntRect.h>
#include <WebCore/PlatformDisplay.h>
#include <WebCore/ShareableBitmap.h>
#include <Rect.h>

namespace WebKit {
using namespace WebCore;

// BackingStore stores and updates a bitmap of the rendered webpage.

BackingStore::BackingStore(const WebCore::IntSize& size, float deviceScaleFactor)
    : m_size(size)
    , m_deviceScaleFactor(deviceScaleFactor)
    , m_bitmap(BRect(0, 0, size.width() * deviceScaleFactor - 1, size.height() * deviceScaleFactor - 1), B_RGBA32, true)
    , m_view(m_bitmap.Bounds(), "BackingStore", 0, 0)
{
    m_bitmap.AddChild(&m_view);
}

BackingStore::~BackingStore()
{
    m_bitmap.RemoveChild(&m_view);
}

void BackingStore::paint(BView* into, const WebCore::IntRect& rect)
{
    // Paint the contents of our bitmap into the BView.
    // The backing store bitmap is scaled by m_deviceScaleFactor (physical pixels),
    // while the destination BView and rect are in logical coordinates.
    // We must scale the source rectangle to match the physical backing store.

    BRect srcRect(rect);
    srcRect.left *= m_deviceScaleFactor;
    srcRect.top *= m_deviceScaleFactor;
    srcRect.right = (srcRect.right + 1) * m_deviceScaleFactor - 1;
    srcRect.bottom = (srcRect.bottom + 1) * m_deviceScaleFactor - 1;

    into->PushState();
    into->SetDrawingMode(B_OP_COPY);
    into->DrawBitmap(&m_bitmap, srcRect, rect);
    into->PopState();
}

void BackingStore::incorporateUpdate(UpdateInfo&& updateInfo)
{
    // Take the changes given in updateInfo and incorporate them into our
    // bitmap. This can involve scrolling our bitmap and copying rectangles
    // from a bitmap containing updates into our bitmap.
    // This implementation is adapted from BackingStoreCairo.

    ASSERT(m_size == updateInfo.viewSize);

    scroll(updateInfo.scrollRect, updateInfo.scrollOffset);

    if (!updateInfo.bitmapHandle)
        return;

    auto bitmapData = ShareableBitmap::create(std::move(*updateInfo.bitmapHandle));
    if (!bitmapData)
        return;
    auto bitmap = bitmapData->createPlatformImage();

    IntPoint updateRectLocation = updateInfo.updateRectBounds.location();

    if (m_bitmap.Lock()) {
        m_view.PushState();
        for (const auto& updateRect : updateInfo.updateRects) {
            IntRect srcRect = updateRect;
            srcRect.move(-updateRectLocation.x(), -updateRectLocation.y());
            // DrawBitmap draws from source to destination.
            // srcRect is in the coordinate system of the 'bitmap' (the update tile).
            // updateRect is in the coordinate system of 'm_view' (the backing store).
            m_view.DrawBitmap(bitmap.get(), srcRect, updateRect);
        }
        m_view.Sync();
        m_view.PopState();
        m_bitmap.Unlock();
    }
}

void BackingStore::scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset)
{
    // Shift the content inside of scrollRect by scrollOffset. Any existing
    // part of the bitmap that ends up outside of the scrollRect will be
    // clipped. It doesn't matter what is done with newly-exposed regions that
    // didn't exist before.
    // This implementation is adapted from BackingStoreCairo.

    if (scrollOffset.isZero())
        return;

    IntRect targetRect = scrollRect;
    targetRect.move(scrollOffset);
    targetRect.intersect(scrollRect);

    if (targetRect.isEmpty()) {
        // Everything is scrolled off the screen. It doesn't matter what we
        // do with the space that was left behind. Let's just leave everything
        // as it was!
        return;
    }

    IntRect sourceRect = targetRect;
    sourceRect.move(-scrollOffset);

    if (m_bitmap.Lock()) {
        m_view.CopyBits(sourceRect, targetRect);
        m_view.Sync();
        m_bitmap.Unlock();
    }
}

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
