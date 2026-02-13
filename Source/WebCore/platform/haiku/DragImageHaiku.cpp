/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
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

#include "BitmapImage.h"
#include "FontCascade.h"
#include "Image.h"
#include "NotImplemented.h"

#include <Bitmap.h>
#include <View.h>
#include <cstring>

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

IntSize dragImageSize(DragImageRef image)
{
    if (!image)
        return IntSize();
    BRect r = image->Bounds();
    return IntSize(r.IntegerWidth() + 1, r.IntegerHeight() + 1);
}

void deleteDragImage(DragImageRef image)
{
    delete image;
}

DragImageRef scaleDragImage(DragImageRef image, FloatSize scale)
{
    if (!image)
        return nullptr;

    if (scale.width() == 1 && scale.height() == 1)
        return image;

    BRect oldRect = image->Bounds();
    BRect newRect(0, 0, oldRect.Width() * scale.width(), oldRect.Height() * scale.height());

    BBitmap* newBitmap = new BBitmap(newRect, image->ColorSpace(), true);
    if (newBitmap->InitCheck() != B_OK) {
        delete newBitmap;
        delete image;
        return nullptr;
    }

    BView* view = new BView(newRect, "drawing", B_FOLLOW_ALL, 0);
    newBitmap->AddChild(view);
    if (newBitmap->Lock()) {
        view->DrawBitmap(image, newRect);
        view->Sync();
        newBitmap->RemoveChild(view);
        newBitmap->Unlock();
    }
    delete view;
    delete image;
    return newBitmap;
}

DragImageRef dissolveDragImageToFraction(DragImageRef image, float delta)
{
    if (!image)
        return nullptr;

    if (delta == 1.0f)
        return image;

    uint8* bits = (uint8*)image->Bits();
    int32 length = image->BitsLength();

    // Assume B_RGBA32 (BGRA)
    for (int32 i = 0; i < length; i += 4) {
        bits[i + 3] = (uint8)(bits[i + 3] * delta);
    }

    return image;
}

DragImageRef createDragImageFromImage(Image* image, ImageOrientation orientation, GraphicsClient*, float deviceScaleFactor)
{
    if (!image)
        return nullptr;

    auto nativeImage = image->nativeImage();
    if (!nativeImage)
        return nullptr;

    const BBitmap* source = nativeImage->platformImage();
    if (!source)
        return nullptr;

    return new BBitmap(source);
}

DragImageData createDragImageForLink(Element&, URL& url, const String& label, float deviceScaleFactor)
{
    notImplemented();
    return { nullptr, nullptr };
}

DragImageRef createDragImageIconForCachedImageFilename(const String&)
{
    notImplemented();
    return nullptr;
}

DragImageRef platformAdjustDragImageForDeviceScaleFactor(DragImageRef image, float deviceScaleFactor)
{
    return image;
}

DragImageRef createDragImageForColor(const Color& color, const FloatRect& rect, float, Path&)
{
    BRect r(0, 0, rect.width(), rect.height());
    BBitmap* bitmap = new BBitmap(r, B_RGBA32);
    if (bitmap->InitCheck() == B_OK) {
        auto srgba = color.toSRGB<uint8_t>();
        uint8* bits = (uint8*)bitmap->Bits();
        int32 length = bitmap->BitsLength();
        for (int32 i = 0; i < length; i += 4) {
            bits[i] = srgba.blue;
            bits[i+1] = srgba.green;
            bits[i+2] = srgba.red;
            bits[i+3] = srgba.alpha;
        }
    } else {
        delete bitmap;
        return nullptr;
    }
    return bitmap;
}

}
