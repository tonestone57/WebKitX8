/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FontCache.h"

#include "Font.h"
#include "FontPlatformData.h"
#include "NotImplemented.h"
#include "Font.h"
#include <Font.h>
#include <String.h>
#include <interface/Font.h>

namespace WebCore {

void FontCache::platformInit()
{
}


static bool fontHasGlyphs(const font_family& family, const StringView& text)
{
    BFont font;
    if (font.SetFamilyAndStyle(family, nullptr) != B_OK)
        return false;

    String string = text.toString();
    CString utf8 = string.utf8();
    // Check if the font has the first character of the cluster
    // This is a simplification, but often sufficient for fallback
    bool hasGlyph = false;
    font.GetHasGlyphs(utf8.data(), 1, &hasGlyph);
    return hasGlyph;
}

RefPtr<Font> FontCache::systemFallbackForCharacterCluster(const FontDescription& description,
	const Font& /*originalFontData*/, WebCore::IsForPlatformFont,
	WebCore::FontCache::PreferColoredFont, StringView text)
{
    // List of fallback families to try
    const char* fallbackFamilies[] = {
        "Noto Sans",
        "Noto Serif",
        "Noto Sans CJK JP",
        "Noto Sans CJK SC",
        "Noto Sans CJK TC",
        "Noto Sans CJK KR",
        "Noto Sans Symbols",
        "DejaVu Sans",
        "VL Gothic",
        "IPAGothic",
        "WenQuanYi Micro Hei",
        "Sans"
    };

    // Try to find a font that supports the character
    for (const char* family : fallbackFamilies) {
        font_family bFamily;
        if (BFont().SetFamilyAndStyle(family, nullptr) == B_OK) {
             strncpy(bFamily, family, B_FONT_FAMILY_LENGTH);
             if (fontHasGlyphs(bFamily, text)) {
                 FontPlatformData data(description, AtomString::fromUTF8(family));
                 return fontForPlatformData(data);
             }
        }
    }

    FontPlatformData data(description, AtomString::fromUTF8("Sans"));
    return fontForPlatformData(data);
}

Vector<String> FontCache::systemFontFamilies()
{
    Vector<String> fontFamilies;
    font_family family;
    font_style style;

    be_plain_font->GetFamilyAndStyle(&family, &style);
    fontFamilies.append(String::fromUTF8(family));
    be_bold_font->GetFamilyAndStyle(&family, &style);
    fontFamilies.append(String::fromUTF8(family));
    be_fixed_font->GetFamilyAndStyle(&family, &style);
    fontFamilies.append(String::fromUTF8(family));

    return fontFamilies;
}

bool FontCache::isSystemFontForbiddenForEditing(const String&)
{
    return false;
}

Ref<Font> FontCache::lastResortFallbackFont(const FontDescription& fontDescription)
{
    font_family family;
    font_style style;
    be_plain_font->GetFamilyAndStyle(&family, &style);
    AtomString plainFontFamily = AtomString::fromUTF8(family);
    return *fontForFamily(fontDescription, plainFontFamily);
}

std::unique_ptr<FontPlatformData> FontCache::createFontPlatformData(
    const FontDescription& fontDescription, const AtomString& family,
    const WebCore::FontCreationContext&, WTF::OptionSet<WebCore::FontLookupOptions>)
{
    return std::make_unique<FontPlatformData>(fontDescription, family);
}


Vector<FontSelectionCapabilities> FontCache::getFontSelectionCapabilitiesInFamily(const AtomString& familyName, AllowUserInstalledFonts)
{
    Vector<FontSelectionCapabilities> result;

    int32 count = count_font_styles(familyName.string().utf8().data());

    result.reserveInitialCapacity(count);

    for (int index = 0; index < count; index++)
    {
        font_style nativeStyle;
        uint32 flags = 0;
        if (get_font_style(familyName.string().utf8().data(), index, &nativeStyle, &flags) == B_OK) {
            FontSelectionCapabilities capabilities;

            String style = String::fromUTF8(nativeStyle);

            // Weight
            int weight = 400;
            if (style.containsIgnoringASCIICase("Thin"_s)) weight = 100;
            else if (style.containsIgnoringASCIICase("Extra Light"_s) || style.containsIgnoringASCIICase("Ultra Light"_s)) weight = 200;
            else if (style.containsIgnoringASCIICase("Light"_s)) weight = 300;
            else if (style.containsIgnoringASCIICase("Medium"_s)) weight = 500;
            else if (style.containsIgnoringASCIICase("Semi Bold"_s) || style.containsIgnoringASCIICase("Demi Bold"_s)) weight = 600;
            else if (style.containsIgnoringASCIICase("Bold"_s)) weight = 700;
            else if (style.containsIgnoringASCIICase("Extra Bold"_s) || style.containsIgnoringASCIICase("Ultra Bold"_s)) weight = 800;
            else if (style.containsIgnoringASCIICase("Black"_s) || style.containsIgnoringASCIICase("Heavy"_s)) weight = 900;

            capabilities.weight = { FontSelectionValue(weight), FontSelectionValue(weight), FontSelectionValue(weight) };

            // Width/Stretch
            int width = 100; // Normal
            // FontSelectionValue for width: UltraCondensed = 50, Normal = 100, UltraExpanded = 200?
            // WebKit uses: UltraCondensed=50, ExtraCondensed=62.5, Condensed=75, SemiCondensed=87.5, Normal=100...
            if (style.containsIgnoringASCIICase("Ultra Condensed"_s)) width = 50;
            else if (style.containsIgnoringASCIICase("Extra Condensed"_s)) width = 63;
            else if (style.containsIgnoringASCIICase("Condensed"_s)) width = 75;
            else if (style.containsIgnoringASCIICase("Semi Condensed"_s)) width = 88;
            else if (style.containsIgnoringASCIICase("Semi Expanded"_s)) width = 113;
            else if (style.containsIgnoringASCIICase("Expanded"_s)) width = 125;
            else if (style.containsIgnoringASCIICase("Extra Expanded"_s)) width = 150;
            else if (style.containsIgnoringASCIICase("Ultra Expanded"_s)) width = 200;

            capabilities.width = { FontSelectionValue(width), FontSelectionValue(width), FontSelectionValue(width) };

            // Slope
            if (style.containsIgnoringASCIICase("Italic"_s) || style.containsIgnoringASCIICase("Oblique"_s)) {
                capabilities.slope = { FontSelectionValue::italic(), FontSelectionValue::italic(), FontSelectionValue::italic() };
            } else {
                 capabilities.slope = { FontSelectionValue::normal(), FontSelectionValue::normal(), FontSelectionValue::normal() };
            }

            result.append(capabilities);
        }
    }
    return result;
}


ASCIILiteral FontCache::platformAlternateFamilyName(const String& familyName)
{
    return { };
}

void FontCache::platformInvalidate()
{
}


} // namespace WebCore

