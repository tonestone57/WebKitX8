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

#include "FontCascade.h"
#include "FontDescription.h"
#include "FontSelector.h"
#include "GraphicsContext.h"
#include "TextEncoding.h"
#include "NotImplemented.h"
#include <wtf/text/CString.h>
#include <Font.h>
#include <Shape.h>
#include <String.h>
#include <UnicodeChar.h>
#include <View.h>

#include "PathHaiku.h"

namespace WebCore {

void Font::platformInit()
{
    const BFont* font = m_platformData.font();
    if (!font)
        return;

    font_height height;
    font->GetHeight(&height);
    m_fontMetrics.setAscent(height.ascent);
    m_fontMetrics.setDescent(height.descent);
    m_fontMetrics.setLineSpacing(height.ascent + height.descent);
    m_fontMetrics.setLineGap(height.leading);

    BRect rect;
    font->GetBoundingBoxesAsGlyphs("x", 1, B_SCREEN_METRIC, &rect);

    m_fontMetrics.setXHeight(rect.Height() / 1.25);
        // FIXME we shouldn't need to divide here, but it passes this test:
        // css2.1/20110323/c541-word-sp-000.htm
}

void Font::platformCharWidthInit()
{
    m_avgCharWidth = 0.f;
    m_maxCharWidth = 0.f;
    initCharWidths();
}

void Font::platformDestroy()
{
}

RefPtr<Font> Font::platformCreateScaledFont(const FontDescription& fontDescription, float scaleFactor) const
{
    const float scaledSize = lroundf(fontDescription.computedSize() * scaleFactor);
    return Font::create(FontPlatformData::cloneWithSize(m_platformData, scaledSize));
}

RefPtr<Font> Font::platformCreateHalfWidthFont() const
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=281333 : implement half width font for this platform.
    return nullptr;
}

void Font::determinePitch()
{
    m_treatAsFixedPitch = m_platformData.font() && m_platformData.font()->IsFixed();
}

FloatRect Font::platformBoundsForGlyph(Glyph glyph) const
{
    const BFont* font = m_platformData.font();
    if (!font)
        return FloatRect();

    if (glyph == 0)
        glyph = 0xfdd1;

    BRect rect;
    char string[5] = { 0 };
    char* ptr = string;
    BUnicodeChar::ToUTF8((uint32)glyph, &ptr);
    font->GetBoundingBoxesAsGlyphs(string, 1, B_SCREEN_METRIC, &rect);
    return rect;
}

float Font::platformWidthForGlyph(Glyph glyph) const
{
    if (!platformData().size())
        return 0;

    if (glyph == 0)
        glyph = 0xfdd1;

    float escapements[1];
    char string[5] = { 0 };
    char* ptr = string;
    BUnicodeChar::ToUTF8((uint32)glyph, &ptr);
    m_platformData.font()->GetEscapements(string, 1, escapements);
    return escapements[0] * m_platformData.font()->Size();
}

bool Font::platformSupportsCodePoint(char32_t character, std::optional<char32_t> variation) const
{
    return variation ? false : glyphForCharacter(character);
}

void FontCascade::drawGlyphs(GraphicsContext& graphicsContext, const Font& font,
    std::span<const GlyphBufferGlyph> glyphs, std::span<const GlyphBufferAdvance> advances,
    const FloatPoint& point, WebCore::FontSmoothingMode smoothing)
{
    BView* view = graphicsContext.platformContext();
    view->PushState();

    rgb_color color = graphicsContext.fillColor();

    if (color.alpha < 255 || graphicsContext.isInTransparencyLayer())
        view->SetDrawingMode(B_OP_ALPHA);
    else
        view->SetDrawingMode(B_OP_OVER);
    view->SetHighColor(color);

    // We assume 'font' is valid reference as per C++ semantics.
    BFont bfont = *font.platformData().font();

    if (smoothing == FontSmoothingMode::None)
        bfont.SetFlags(B_DISABLE_ANTIALIASING);
    else
        bfont.SetFlags(B_FORCE_ANTIALIASING);
    view->SetFont(&bfont);

    BPoint offsets[glyphs.size()];
    char buffer[4];
    BString utf8;
    int32 realGlyphCount = 0;
    float offset = point.x();
    for (unsigned i = 0; i < glyphs.size(); i++) {
        Glyph glyph = glyphs[i];
        if (glyph == 0) {
            if (advances[i].width() == 0.0) {
                // These are fake glyphs to keep GlyphBuffer vectors in sync with text runs
                // when a surrogate pair is found (cf. addToGlyphBuffer in WidthIterator.cpp).
                continue;
            }
            glyph = 0xfdd1;
        }

        offsets[realGlyphCount].x = offset;
        offsets[realGlyphCount].y = point.y();
        offset += advances[i].width();

        char* tmp = buffer;
        BUnicodeChar::ToUTF8(glyph, &tmp);
        utf8.Append(buffer, tmp - buffer);
        realGlyphCount++;
    }

    view->DrawString(utf8, offsets, realGlyphCount);
    view->PopState();
}

Path Font::platformPathForGlyph(Glyph glyph) const
{
    const BFont* font = platformData().font();
    if (!font)
        return Path();

    BFont bfont = *font;
    BShape shape;
    char buffer[4];
    char* tmp = buffer;
    BUnicodeChar::ToUTF8(glyph, &tmp);

    // Note: GetGlyphShapes takes char array and BShape* array.
    BShape* shapes[1] = { &shape };
    if (bfont.GetGlyphShapes(buffer, 1, shapes) != B_OK)
        return Path();

    return Path(PathHaiku::create(shape));
}

bool FontCascade::canUseGlyphDisplayList(const RenderStyle&)
{
    return true;
}

ResolvedEmojiPolicy FontCascade::resolveEmojiPolicy(FontVariantEmoji fontVariantEmoji, char32_t)
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=259205 We can't return RequireText or RequireEmoji
    // unless we have a way of knowing whether a font/glyph is color or not.
    switch (fontVariantEmoji) {
    case FontVariantEmoji::Normal:
    case FontVariantEmoji::Unicode:
        return ResolvedEmojiPolicy::NoPreference;
    case FontVariantEmoji::Text:
        return ResolvedEmojiPolicy::RequireText;
    case FontVariantEmoji::Emoji:
        return ResolvedEmojiPolicy::RequireEmoji;
    }
    return ResolvedEmojiPolicy::NoPreference;
}

} // namespace WebCore
