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

FloatRect screenRect(Widget*)
{
    BScreen screen;
    if (screen.IsValid()) {
        BRect frame = screen.Frame();
        return FloatRect(frame.left, frame.top, frame.Width() + 1, frame.Height() + 1);
    }
    return FloatRect(0, 0, 1920, 1080);
}

FloatRect screenAvailableRect(Widget* widget)
{
    // FIXME: We could use the get_deskbar_frame() function
    // from InterfaceDefs.h to make this smaller
    return screenRect(widget);
}

bool screenSupportsExtendedColor(Widget*)
{
    return false;
}

int screenDepth(Widget*)
{
    BScreen screen(B_MAIN_SCREEN_ID);
    if (!screen.IsValid())
        return 24;

    switch (screen.ColorSpace()) {
        case B_RGBA32:
        case B_RGB32:
        case B_RGB24:
            return 24;
        case B_RGB16:
        case B_RGB15:
        case B_RGBA15:
            return 16;
        case B_CMAP8:
            return 8;
        case B_GRAY8:
            return 8;
        case B_GRAY1:
            return 1;
        default:
            return 24;
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
        case B_RGBA15:
            return 5;
        default:
            return 8;
    }
}

bool screenIsMonochrome(Widget*)
{
    BScreen screen(B_MAIN_SCREEN_ID);
    if (!screen.IsValid())
        return false;
    return screen.ColorSpace() == B_GRAY1 || screen.ColorSpace() == B_MONOCHROME_1_BIT;
}

DestinationColorSpace screenColorSpace(Widget*)
{
    return DestinationColorSpace::SRGB();
}

} // namespace WebCore
