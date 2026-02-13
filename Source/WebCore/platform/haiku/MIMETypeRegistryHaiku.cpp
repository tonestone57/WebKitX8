/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Trolltech ASA
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
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
#include "MIMETypeRegistry.h"

#include <MimeType.h>
#include <String.h>

#include <wtf/Assertions.h>
#include <wtf/MainThread.h>
#include <wtf/text/CString.h>

namespace WebCore {
struct ExtensionMap {
    const char* extension;
    const char* mimeType;
};

static const ExtensionMap extensionMap[] = {
    { "bmp", "image/bmp" },
    { "gif", "image/gif" },
    { "hpkg", "application/x-vnd.haiku-package" },
    { "html", "text/html" },
    { "ico", "image/x-icon" },
    { "jpeg", "image/jpeg" },
    { "jpg", "image/jpeg" },
    { "js", "application/x-javascript" },
    { "pdf", "application/pdf" },
    { "png", "image/png" },
    { "rss", "application/rss+xml" },
    { "svg", "image/svg+xml" },
    { "text", "text/plain" },
    { "txt", "text/plain" },
    { "xbm", "image/x-xbitmap" },
    { "xml", "text/xml" },
    { "xsl", "text/xsl" },
    { "xhtml", "application/xhtml+xml" },
    { 0, 0 }
};

String MIMETypeRegistry::mimeTypeForExtension(const StringView ext)
{
    String str = ext.toString();

    // WebCore built-in types usually cover a lot, but let's check Haiku system DB first
    // or as a robust fallback. The previous code checked built-in first.
    // Let's stick to built-in first for consistency with other ports,
    // but the map was very small.

    // Improve lookup by using Haiku's BMimeType::GuessMimeType properly
    // It works well with extensions if we provide a filename.

    BString fakeFileName("filename.");
    fakeFileName.Append(str.utf8().data());

    BMimeType type;
    if (BMimeType::GuessMimeType(fakeFileName.String(), &type) == B_OK) {
        // Haiku might return "application/octet-stream" if unknown, which we might want to avoid
        // if we want to fallback to other mechanisms?
        if (strcmp(type.Type(), "application/octet-stream") != 0)
             return String::fromUTF8(type.Type());
    }

    // Try WebCore built-in types if system failed or returned generic
    String lowerExt = ext.convertToASCIILowercase();
    const ExtensionMap* extMap = extensionMap;
    while (extMap->extension) {
        if (lowerExt == StringView::fromLatin1(extMap->extension))
            return String::fromUTF8(extMap->mimeType);
        ++extMap;
    }

    return String();
}


String MIMETypeRegistry::preferredExtensionForMIMEType(const String& type)
{
    BMimeType mimeType(type.utf8().data());
    BMessage storage;
    if (mimeType.GetFileExtensions(&storage) != B_OK)
        return String();

    const char* extension;
    if (storage.FindString("extensions", 0, &extension) == B_OK)
        return String::fromUTF8(extension);

    return String();
}

Vector<String> MIMETypeRegistry::extensionsForMIMEType(const String& type)
{
    BMimeType mimeType(type.utf8().data());
    BMessage storage;
    if (mimeType.GetFileExtensions(&storage) != B_OK)
        return { };

    Vector<String> extensions;
    const char* extension;
    for (int32 i = 0; storage.FindString("extensions", i, &extension) == B_OK; i++)
        extensions.append(String::fromUTF8(extension));

    return extensions;
}

bool MIMETypeRegistry::isApplicationPluginMIMEType(const String&)
{
    // Haiku does not support NPAPI plugins.
    return false;
}

} // namespace WebCore
