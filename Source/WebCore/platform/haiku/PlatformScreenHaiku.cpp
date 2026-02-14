/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 *
 * All rights reserved.
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
#include "Screen.h"

#include "FloatRect.h"
#include "Frame.h"
#include "FrameView.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "Page.h"
#include "Widget.h"
#include <GraphicsDefs.h>
#include <interface/Screen.h>
#include <interface/Deskbar.h>


namespace WebCore {

int screenHorizontalDPI(Widget*)
{
    return 96;
}

int screenVerticalDPI(Widget*)
{
    return 96;
}

bool screenHasInvertedColors()
{
    return false;
}

FloatRect screenRect(Widget* widget)
{
    BScreen screen;
    if (widget) {
        if (BView* view = widget->platformWidget()) {
            if (BWindow* window = view->Window())
                screen = BScreen(window);
        }
    }

    if (!screen.IsValid())
        return FloatRect();
    // BRect is inclusive, so add 1 to width and height
    BRect frame = screen.Frame();
    return FloatRect(frame.left, frame.top, frame.Width() + 1, frame.Height() + 1);
}

FloatRect screenAvailableRect(Widget* widget)
{
    BScreen screen;
    if (widget) {
        if (BView* view = widget->platformWidget()) {
            if (BWindow* window = view->Window())
                screen = BScreen(window);
        }
    }

    if (!screen.IsValid())
        return FloatRect();

    // BRect is inclusive, so add 1 to width and height
    BRect frame = screen.Frame();

    // Subtract deskbar frame if it exists and intersects
    BDeskbar deskbar;
    BRect deskbarFrame = deskbar.Frame();

    if (deskbarFrame.IsValid() && frame.Intersects(deskbarFrame)) {
        if (deskbarFrame.top <= frame.top && deskbarFrame.bottom >= frame.bottom) {
             // Vertical deskbar
             if (deskbarFrame.left <= frame.left)
                 frame.left = deskbarFrame.right + 1;
             else
                 frame.right = deskbarFrame.left - 1;
        } else if (deskbarFrame.left <= frame.left && deskbarFrame.right >= frame.right) {
             // Horizontal deskbar
             if (deskbarFrame.top <= frame.top)
                 frame.top = deskbarFrame.bottom + 1;
             else
                 frame.bottom = deskbarFrame.top - 1;
        }
    }

    return FloatRect(frame.left, frame.top, frame.Width() + 1, frame.Height() + 1);
}

bool screenSupportsExtendedColor(Widget* widget)
{
    BScreen screen;
    if (widget) {
        if (BView* view = widget->platformWidget()) {
            if (BWindow* window = view->Window())
                screen = BScreen(window);
        }
    }

    if (!screen.IsValid())
        return false;

    // Check if the screen is using a wide gamut color space.
    // Haiku's BScreen doesn't explicitly expose HDR/WideGamut flags easily yet,
    // but we can check the color space.
    // For now, return false as standard Haiku screens are sRGB.
    return false;
}

int screenDepth(Widget*)
{
    BScreen screen;
    if (!screen.IsValid())
        return 8;

    switch (screen.ColorSpace()) {
    case B_RGBA32:
    case B_RGB32:
        return 32;
    case B_RGB24:
        return 24;
    case B_RGB16:
        return 16;
    case B_RGB15:
        return 15;
    case B_CMAP8:
    case B_GRAY8:
        return 8;
    case B_GRAY1:
    case B_MONOCHROME_1_BIT:
        return 1;
    default:
        return 24; // Default to 24-bit
    }
}

int screenDepthPerComponent(Widget*)
{
    BScreen screen(B_MAIN_SCREEN_ID);
    if (!screen.IsValid())
        return 8;

    switch (screen.ColorSpace()) {
        case B_RGBA32:
        case B_RGB32:
        case B_RGB24:
            return 8;
        case B_RGB16:
        case B_RGB15:
            return 5;
        case B_CMAP8:
        case B_GRAY8:
            return 8;
        case B_GRAY1:
        case B_MONOCHROME_1_BIT:
            return 1;
        default:
            return 8;
    }
}

bool screenIsMonochrome(Widget*)
{
    BScreen screen;
    if (!screen.IsValid())
        return false;
    return screen.ColorSpace() == B_MONOCHROME_1_BIT || screen.ColorSpace() == B_GRAY1;
}

DestinationColorSpace screenColorSpace(Widget*)
{
    return DestinationColorSpace::SRGB();
}

} // namespace WebCore
