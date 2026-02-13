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


RefPtr<Font> FontCache::systemFallbackForCharacterCluster(const FontDescription& description,
	const Font& /*originalFontData*/, WebCore::IsForPlatformFont,
	WebCore::FontCache::PreferColoredFont, StringView)
{
    FontPlatformData data(description, AtomString::fromUTF8("Sans"));
        // FIXME check that the requested characters are actually available,
        // and try to use the other info in the arguments (should this be
        // monospace, etc)
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
            // Map Haiku font style to FontSelectionCapabilities
            // Haiku doesn't expose weight/stretch/slope directly in a standard way except via name or flags
            // But for now, we can try to guess or just return default capabilities if we can't parse it.
            // Actually, FontSelectionCapabilities expects weight/width/slope ranges.

            // FIXME: Properly parse style name or flags to Determine weight/width/slope.
            // For now, we will add a default capability which implies the font is available.
            // Or we can try to use BFont to inspect it if possible.

            FontSelectionCapabilities capabilities;

            // Simple heuristics based on style name
            String style = String::fromUTF8(nativeStyle);
            if (style.containsIgnoringASCIICase("Bold"_s)) {
                capabilities.weight = { FontSelectionValue(700), FontSelectionValue(700), FontSelectionValue(700) };
            }
            if (style.containsIgnoringASCIICase("Italic"_s) || style.containsIgnoringASCIICase("Oblique"_s)) {
                capabilities.slope = { FontSelectionValue::italic(), FontSelectionValue::italic(), FontSelectionValue::italic() };
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

