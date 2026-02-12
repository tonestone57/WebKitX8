/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
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
#include "Cursor.h"

#include "Image.h"
#include "IntPoint.h"
#include "NativeImage.h"
#include <WebCore/NotImplemented.h>

#include <app/Cursor.h>
#include <interface/Bitmap.h>
#include <translation/TranslationUtils.h>
#include <wtf/Assertions.h>
#include <wtf/text/CString.h>

namespace WebCore {

Cursor::Cursor(const Cursor& other)
    : m_type(other.m_type)
    , m_image(other.m_image)
    , m_hotSpot(other.m_hotSpot)
    , m_cursor(other.m_cursor ? new BCursor(*other.m_cursor) : 0)
{
}

Cursor::Cursor(Image* image, const IntPoint& hotSpot)
    : m_type(Cursor::Type::Custom)
    , m_image(image)
    , m_hotSpot(hotSpot)
    , m_cursor(0)
{
    if (image) {
        if (auto nativeImage = image->nativeImage()) {
            if (auto* platformImage = nativeImage->platformImage().get()) {
                 m_cursor = new BCursor(platformImage, BPoint(hotSpot.x(), hotSpot.y()));
            }
        }
    }
}

Cursor::Cursor(const String& filename, bool, const IntPoint& hotSpot)
    : m_type(Cursor::Type::Custom)
    , m_image(nullptr)
    , m_hotSpot(hotSpot)
    , m_cursor(nullptr)
{
    BBitmap* bitmap = BTranslationUtils::GetBitmap(filename.utf8().data());
    if (bitmap) {
        m_cursor = new BCursor(bitmap, BPoint(hotSpot.x(), hotSpot.y()));
        delete bitmap;
    }
}

Cursor::~Cursor()
{
    delete m_cursor;
}

Cursor& Cursor::operator=(const Cursor& other)
{
    m_type = other.m_type;
    m_image = other.m_image;
    m_hotSpot = other.m_hotSpot;
    delete m_cursor;
    m_cursor = other.m_cursor ? new BCursor(*other.m_cursor) : 0;
    return *this;
}

void Cursor::ensurePlatformCursor() const
{
    if (m_cursor)
        return;

    switch (m_type) {
    case Cursor::Type::Pointer:
        m_cursor = new BCursor(B_CURSOR_ID_SYSTEM_DEFAULT);
        break;
    case Cursor::Type::Cross:
        m_cursor = new BCursor(B_CURSOR_ID_CROSS_HAIR);
        break;
    case Cursor::Type::Hand:
        m_cursor = new BCursor(B_CURSOR_ID_FOLLOW_LINK);
        break;
    case Cursor::Type::IBeam:
        m_cursor = new BCursor(B_CURSOR_ID_I_BEAM);
        break;
    case Cursor::Type::Wait:
        m_cursor = new BCursor(B_CURSOR_ID_PROGRESS);
        break;
    case Cursor::Type::Help:
        m_cursor = new BCursor(B_CURSOR_ID_HELP);
        break;
    case Cursor::Type::EastResize:
        m_cursor = new BCursor(B_CURSOR_ID_RESIZE_EAST);
        break;
    case Cursor::Type::WestResize:
        m_cursor = new BCursor(B_CURSOR_ID_RESIZE_WEST);
        break;
    case Cursor::Type::EastWestResize:
        m_cursor = new BCursor(B_CURSOR_ID_RESIZE_EAST_WEST);
        break;
    case Cursor::Type::NorthEastResize:
        m_cursor = new BCursor(B_CURSOR_ID_RESIZE_NORTH_EAST);
        break;
    case Cursor::Type::NorthWestResize:
        m_cursor = new BCursor(B_CURSOR_ID_RESIZE_NORTH_WEST);
        break;
    case Cursor::Type::SouthEastResize:
        m_cursor = new BCursor(B_CURSOR_ID_RESIZE_SOUTH_EAST);
        break;
    case Cursor::Type::SouthWestResize:
        m_cursor = new BCursor(B_CURSOR_ID_RESIZE_SOUTH_WEST);
        break;
    case Cursor::Type::NorthSouthResize:
        m_cursor = new BCursor(B_CURSOR_ID_RESIZE_NORTH_SOUTH);
        break;
    case Cursor::Type::NorthEastSouthWestResize:
        m_cursor = new BCursor(B_CURSOR_ID_RESIZE_NORTH_EAST_SOUTH_WEST);
        break;
    case Cursor::Type::NorthWestSouthEastResize:
        m_cursor = new BCursor(B_CURSOR_ID_RESIZE_NORTH_WEST_SOUTH_EAST);
        break;
    case Cursor::Type::ColumnResize:
    case Cursor::Type::RowResize:
        m_cursor = new BCursor(B_CURSOR_ID_SYSTEM_DEFAULT);
        break;
    case Cursor::Type::Move:
        m_cursor = new BCursor(B_CURSOR_ID_MOVE);
        break;
    case Cursor::Type::VerticalText:
        m_cursor = new BCursor(B_CURSOR_ID_I_BEAM); // Fallback
        break;
    case Cursor::Type::Cell:
    case Cursor::Type::ContextMenu:
    case Cursor::Type::Alias:
    case Cursor::Type::Progress:
    case Cursor::Type::NoDrop:
        m_cursor = new BCursor(B_CURSOR_ID_NOT_ALLOWED);
        break;
    case Cursor::Type::Copy:
    case Cursor::Type::None:
    case Cursor::Type::NotAllowed:
        m_cursor = new BCursor(B_CURSOR_ID_NOT_ALLOWED);
        break;
    case Cursor::Type::ZoomIn:
        m_cursor = new BCursor(B_CURSOR_ID_ZOOM_IN);
        break;
    case Cursor::Type::ZoomOut:
        m_cursor = new BCursor(B_CURSOR_ID_ZOOM_OUT);
        break;
    case Cursor::Type::Grab:
        m_cursor = new BCursor(B_CURSOR_ID_GRAB);
        break;
    case Cursor::Type::Grabbing:
        m_cursor = new BCursor(B_CURSOR_ID_GRABBING);
        break;
    case Cursor::Type::Custom:
        break;
    }
}

} // namespace WebCore
