/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "DragImage.h"

#include "CachedImage.h"
#include "FontCascade.h"
#include "Image.h"

namespace WebCore {

const float DragLabelBorderX = 4;
// Keep border_y in synch with DragController::LinkDragBorderInset.
const float DragLabelBorderY = 2;
const float DragLabelRadius = 5;
const float LabelBorderYOffset = 2;

const float MinDragLabelWidthBeforeClip = 120;
const float MaxDragLabelWidth = 200;
const float MaxDragLabelStringWidth = (MaxDragLabelWidth - 2 * DragLabelBorderX);

const float DragLinkLabelFontsize = 11;
const float DragLinkUrlFontSize = 10;

DragImageRef fitDragImageToMaxSize(DragImageRef image, const IntSize& layoutSize, const IntSize& maxSize)
{
    if (!image)
        return nullptr;

    float heightResizeRatio = 0.0f;
    float widthResizeRatio = 0.0f;
    float resizeRatio = -1.0f;
    IntSize originalSize = dragImageSize(image);

    if (layoutSize.width() > maxSize.width()) {
        widthResizeRatio = maxSize.width() / (float)layoutSize.width();
        resizeRatio = widthResizeRatio;
    }

    if (layoutSize.height() > maxSize.height()) {
        heightResizeRatio = maxSize.height() / (float)layoutSize.height();
        if ((resizeRatio < 0.0f) || (resizeRatio > heightResizeRatio))
            resizeRatio = heightResizeRatio;
    }

    if (layoutSize == originalSize)
        return resizeRatio > 0.0f ? scaleDragImage(image, FloatSize(resizeRatio, resizeRatio)) : image;

    return image;
}

DragImageRef scaleDragImage(DragImageRef image, FloatSize scale)
{
    if (!image)
        return nullptr;

    BBitmap* original = static_cast<BBitmap*>(image);
    BRect bounds = original->Bounds();
    BRect newBounds(0, 0, bounds.Width() * scale.width(), bounds.Height() * scale.height());

    BBitmap* scaled = new BBitmap(newBounds, B_RGBA32, true);
    if (scaled->Lock()) {
        BView* view = new BView(newBounds, "drawing", 0, 0);
        scaled->AddChild(view);
        view->SetDrawingMode(B_OP_COPY);
        view->DrawBitmap(original, newBounds);
        scaled->RemoveChild(view);
        delete view;
        scaled->Unlock();
    }
    delete original;
    return scaled;
}

DragImageRef dissolveDragImageToFraction(DragImageRef image, float fraction)
{
    if (!image)
        return nullptr;

    BBitmap* bitmap = static_cast<BBitmap*>(image);
    if (bitmap->Lock()) {
        uint8* bits = (uint8*)bitmap->Bits();
        int32 bpr = bitmap->BytesPerRow();
        int32 height = bitmap->Bounds().IntegerHeight() + 1;
        int32 width = bitmap->Bounds().IntegerWidth() + 1;

        for (int y = 0; y < height; y++) {
            uint8* row = bits + y * bpr;
            for (int x = 0; x < width; x++) {
                row[3] = (uint8)(row[3] * fraction);
                row += 4;
            }
        }
        bitmap->Unlock();
    }
    return bitmap;
}

DragImageRef createDragImageFromImage(Image* image, ImageOrientation)
{
    if (!image)
        return nullptr;

    auto nativeImage = image->nativeImage();
    if (!nativeImage)
        return nullptr;

    return new BBitmap(nativeImage.get());
}

DragImageRef createDragImageForColor(const Color& color, const FloatRect& rect, float, Path&)
{
    BBitmap* bitmap = new BBitmap(rect, B_RGBA32, true);
    if (bitmap->Lock()) {
        BView* view = new BView(rect, "drag", 0, 0);
        bitmap->AddChild(view);
        view->SetHighColor(color);
        view->FillRect(rect);
        bitmap->RemoveChild(view);
        delete view;
        bitmap->Unlock();
    }
    return bitmap;
}

void deleteDragImage(DragImageRef image)
{
    if (image)
        delete static_cast<BBitmap*>(image);
}

IntSize dragImageSize(DragImageRef image)
{
    if (!image)
        return IntSize();

    BRect r = static_cast<BBitmap*>(image)->Bounds();
    return IntSize(r.Width() + 1, r.Height() + 1);
}

DragImageRef createDragImageIconForCachedImageFilename(const String&)
{
    return nullptr;
}

DragImageRef createDragImageForLink(Element&, URL&, const String& label, TextIndicatorData&, FontCascade&, float)
{
    return nullptr;
}

} // namespace WebCore
