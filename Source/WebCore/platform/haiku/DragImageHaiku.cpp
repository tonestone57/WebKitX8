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

#include "BitmapImage.h"
#include "CachedImage.h"
#include "FontCascade.h"
#include "Image.h"
#include "NotImplemented.h"
#include "TextRun.h"

#include <Bitmap.h>
#include <InterfaceDefs.h>
#include <TranslationUtils.h>
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

DragImageRef dissolveDragImageToFraction(DragImageRef image, float fraction)
{
    if (!image)
        return nullptr;

    BBitmap* bitmap = new BBitmap(static_cast<BBitmap*>(image));
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
    delete static_cast<BBitmap*>(image);
    return bitmap;
}

DragImageRef createDragImageFromImage(Image* image, ImageOrientation, GraphicsClient*, float)
{
    if (!image)
        return nullptr;

    auto nativeImage = image->nativeImage();
    if (!nativeImage)
        return nullptr;

    const BBitmap* source = nativeImage->platformImage().get();
    if (!source)
        return nullptr;

    return new BBitmap(source);
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

DragImageRef createDragImageIconForCachedImageFilename(const String& filename)
{
    BBitmap* bitmap = BTranslationUtils::GetBitmap(filename.utf8().data());
    return bitmap;
}

DragImageData createDragImageForLink(Element&, URL& url, const String& label, TextIndicatorData&, FontCascade&, float)
{
    // Simple drag image for links: Label on top, URL on bottom
    BFont font;
    if (be_plain_font)
        font = *be_plain_font;

    font.SetSize(DragLinkLabelFontsize);

    float labelWidth = font.StringWidth(label.utf8().data());
    float urlWidth = font.StringWidth(url.string().utf8().data());
    float width = std::max(labelWidth, urlWidth) + 10;
    float height = DragLinkLabelFontsize + DragLinkUrlFontSize + 10;

    BRect rect(0, 0, width, height);
    BBitmap* bitmap = new BBitmap(rect, B_RGBA32, true);
    if (bitmap->InitCheck() != B_OK) {
        delete bitmap;
        return { nullptr, nullptr };
    }

    BView* view = new BView(rect, "drag", B_FOLLOW_NONE, 0);
    bitmap->AddChild(view);

    if (bitmap->Lock()) {
        view->SetHighColor(B_TRANSPARENT_COLOR);
        view->FillRect(rect);

        view->SetHighColor(0, 0, 0, 255); // Black text
        view->SetFont(&font);

        // Draw Label
        font_height fh;
        font.GetHeight(&fh);
        float y = fh.ascent + 2;
        view->DrawString(label.utf8().data(), BPoint(5, y));

        // Draw URL
        font.SetSize(DragLinkUrlFontSize);
        font.GetHeight(&fh);
        y += fh.ascent + fh.descent + fh.leading + 2;
        view->SetHighColor(0, 0, 255, 255); // Blue URL
        view->SetFont(&font);
        view->DrawString(url.string().utf8().data(), BPoint(5, y));

        view->Sync();
        bitmap->RemoveChild(view);
        bitmap->Unlock();
    }
    delete view;

    return { bitmap, nullptr };
}

} // namespace WebCore
