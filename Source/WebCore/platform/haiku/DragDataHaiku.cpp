/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2009 Stephan Aßmus <superstippi@gmx.de>
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
#include "DragData.h"

#include "Document.h"
#include "DocumentFragment.h"

#include <Entry.h>
#include <Message.h>
#include <Path.h>
#include <String.h>
#include <wtf/text/CString.h>

namespace WebCore {

bool DragData::canSmartReplace() const
{
    return true;
}

bool DragData::containsColor() const
{
    return m_platformDragData && m_platformDragData->HasData("RGBColor", B_RGB_COLOR_TYPE);
}

bool DragData::containsFiles() const
{
    return m_platformDragData && m_platformDragData->HasRef("refs");
}

unsigned DragData::numberOfFiles() const
{
    if (!m_platformDragData)
        return 0;

    type_code type;
    int32 count;
    if (m_platformDragData->GetInfo("refs", &type, &count) == B_OK)
        return count;

    return 0;
}

Vector<String> DragData::asFilenames() const
{
    Vector<String> filenames;
    if (!m_platformDragData)
        return filenames;

    entry_ref ref;
    for (int32 i = 0; m_platformDragData->FindRef("refs", i, &ref) == B_OK; i++) {
        BEntry entry(&ref, true);
        if (entry.InitCheck() == B_OK) {
            BPath path;
            if (entry.GetPath(&path) == B_OK)
                filenames.append(String::fromUTF8(path.Path()));
        }
    }
    return filenames;
}

bool DragData::containsPlainText() const
{
    return m_platformDragData && m_platformDragData->HasData("text/plain", B_MIME_TYPE);
}

String DragData::asPlainText() const
{
    if (!m_platformDragData)
        return String();

    const char* text;
    ssize_t length;
    if (m_platformDragData->FindData("text/plain", B_MIME_TYPE, (const void**)&text, &length) == B_OK)
        return String::fromUTF8(std::span<const char>(text, length));

    return String();
}

Color DragData::asColor() const
{
    if (!m_platformDragData)
        return Color();

    const rgb_color* color;
    ssize_t length;
    if (m_platformDragData->FindData("RGBColor", B_RGB_COLOR_TYPE, (const void**)&color, &length) == B_OK)
        return Color(SRGBA<uint8_t> { color->red, color->green, color->blue, color->alpha });

    return Color();
}

bool DragData::containsCompatibleContent(WebCore::DragData::DraggingPurpose) const
{
    return containsColor() || containsURL() || containsPlainText() || containsFiles();
}

bool DragData::containsURL(FilenameConversionPolicy) const
{
    return m_platformDragData && m_platformDragData->HasData("text/url", B_MIME_TYPE);
}

String DragData::asURL(FilenameConversionPolicy, String* title) const
{
    if (!m_platformDragData)
        return String();

    if (title)
        *title = String();

    const char* url;
    ssize_t length;
    if (m_platformDragData->FindData("text/url", B_MIME_TYPE, (const void**)&url, &length) == B_OK)
        return String::fromUTF8(std::span<const char>(url, length));

    if (m_platformDragData->FindData("text/plain", B_MIME_TYPE, (const void**)&url, &length) == B_OK) {
        String text = String::fromUTF8(std::span<const char>(url, length));
        if (text.contains("://"))
            return text;
    }

    return String();
}

bool DragData::shouldMatchStyleOnDrop() const
{
    return true;
}

} // namespace WebCore
