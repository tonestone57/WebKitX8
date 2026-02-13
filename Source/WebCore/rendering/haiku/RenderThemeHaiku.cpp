/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 *               2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 *
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "RenderThemeHaiku.h"

#include "GraphicsContext.h"
#include "InputTypeNames.h"
#include "NotImplemented.h"
#include "PaintInfo.h"
#include "RenderBox.h"
#include "RenderElement.h"
#include "RenderMeter.h"
#include "RenderProgress.h"
#include "RenderStyle+SettersInlines.h"
#include "UserAgentScripts.h"
#include "UserAgentStyleSheets.h"
#include <ControlLook.h>
#include <View.h>
#include <private/interface/DefaultColors.h>

#include <wtf/text/StringBuilder.h>


namespace WebCore {

static const int sliderThumbWidth = 15;
static const int sliderThumbHeight = 17;

RenderTheme& RenderTheme::singleton()
{
    static NeverDestroyed<RenderThemeHaiku> theme;
    return theme;
}

RenderThemeHaiku::RenderThemeHaiku()
{
}

RenderThemeHaiku::~RenderThemeHaiku()
{
}

bool RenderThemeHaiku::paintSliderTrack(const RenderElement& object, const PaintInfo& info, const FloatRect& intRect)
{
    rgb_color base = colorForValue(B_CONTROL_BACKGROUND_COLOR, object.useDarkAppearance());
    rgb_color background = base;
        // TODO: From PaintInfo?
    BRect rect = intRect;
    BView* view = info.context().platformContext();
    unsigned flags = flagsForObject(object);
    if (isPressed(object))
    	flags |= BControlLook::B_ACTIVATED;
    if (isDefault(object))
    	flags |= BControlLook::B_DEFAULT_BUTTON;
    be_control_look->DrawSliderBar(view, rect, view->Bounds(), base, background, flags,
        object.style().appearance() == StyleAppearance::SliderHorizontal ?
            B_HORIZONTAL : B_VERTICAL);

#if ENABLE(DATALIST_ELEMENT)
    paintSliderTicks(object, info, intRect);
#endif

    return false;
}

void RenderThemeHaiku::adjustSliderTrackStyle(RenderStyle& style, const Element*) const
{
    style.setBoxShadow(CSS::Keyword::None {});
}

void RenderThemeHaiku::adjustSliderThumbStyle(RenderStyle& style, const Element* element) const
{
    RenderTheme::adjustSliderThumbStyle(style, element);
    style.setBoxShadow(CSS::Keyword::None {});
}

void RenderThemeHaiku::adjustSliderThumbSize(RenderStyle& style, const Element*) const
{
    const StyleAppearance& appearance = style.appearance();
    if (appearance == StyleAppearance::SliderVertical) {
        style.setWidth(WebCore::Style::PreferredSize::Fixed { sliderThumbHeight });
        style.setHeight(WebCore::Style::PreferredSize::Fixed { sliderThumbWidth });
    } else if (appearance == StyleAppearance::SliderHorizontal) {
        style.setWidth(WebCore::Style::PreferredSize::Fixed {sliderThumbWidth });
        style.setHeight(WebCore::Style::PreferredSize::Fixed { sliderThumbHeight });
    }
}

#if ENABLE(DATALIST_ELEMENT)
IntSize RenderThemeHaiku::sliderTickSize() const
{
    return IntSize(1, 6);
}

int RenderThemeHaiku::sliderTickOffsetFromTrackCenter() const
{
    static const int sliderTickOffset = -(sliderThumbHeight / 2 + 1);

    return sliderTickOffset;
}

#endif

bool RenderThemeHaiku::paintSliderThumb(const RenderElement& object, const PaintInfo& info, const FloatRect& intRect)
{
    rgb_color base = colorForValue(B_CONTROL_BACKGROUND_COLOR, object.useDarkAppearance());
    BRect rect = intRect;
    BView* view = info.context().platformContext();
    unsigned flags = flagsForObject(object);
    if (isPressed(object))
    	flags |= BControlLook::B_ACTIVATED;
    if (isDefault(object))
    	flags |= BControlLook::B_DEFAULT_BUTTON;
    be_control_look->DrawSliderThumb(view, rect, view->Bounds(), base, flags,
        object.style().appearance() == StyleAppearance::SliderHorizontal ?
            B_HORIZONTAL : B_VERTICAL);

    return false;
}


#if ENABLE(VIDEO)
Vector<String, 2> RenderThemeHaiku::mediaControlsScripts()
{
#if ENABLE(MODERN_MEDIA_CONTROLS)
    return { StringImpl::createWithoutCopying(std::span<const char>(ModernMediaControlsJavaScript, sizeof(ModernMediaControlsJavaScript))) };
#else
    return { };
#endif
}
#endif

void RenderThemeHaiku::adjustTextFieldStyle(RenderStyle&, const Element*) const
{
}

bool RenderThemeHaiku::paintTextField(const RenderElement& object, const PaintInfo& info, const FloatRect& intRect)
{
    if (info.context().paintingDisabled())
        return true;

    if (!be_control_look)
        return true;

    rgb_color base = colorForValue(B_CONTROL_BACKGROUND_COLOR, object.useDarkAppearance());
    //rgb_color background = base;
        // TODO: From PaintInfo?
    BRect rect(intRect);
    BView* view(info.context().platformContext());
    unsigned flags = flagsForObject(object) & ~BControlLook::B_CLICKED;

    view->PushState();
    be_control_look->DrawTextControlBorder(view, rect, view->Bounds(), base, flags);
    view->PopState();
    return false;
}

void RenderThemeHaiku::adjustTextAreaStyle(RenderStyle& style, const Element* element) const
{
	adjustTextFieldStyle(style, element);
}

bool RenderThemeHaiku::paintTextArea(const RenderElement& object, const PaintInfo& info, const FloatRect& intRect)
{
    return paintTextField(object, info, intRect);
}

bool RenderThemeHaiku::paintMenuList(const RenderElement& object, const PaintInfo& info, const FloatRect& rect)
{
    if (!be_control_look)
        return true;

    // A MenuList is just a button in Haiku (BMenuField)
    return paintButton(object, info, rect);
}

bool RenderThemeHaiku::paintMeter(const RenderElement& object, const PaintInfo& info, const FloatRect& intRect)
{
    if (!be_control_look)
        return true;

    if (!is<RenderMeter>(object))
        return true;

    // Meter is similar to progress bar
    const auto& renderMeter = downcast<RenderMeter>(object);
    // TODO: Use different colors based on meter value/optimality?
    double position = renderMeter.valueRatio();

    rgb_color base = colorForValue(B_CONTROL_BACKGROUND_COLOR, object.useDarkAppearance());
    rgb_color barColor = colorForValue(B_CONTROL_HIGHLIGHT_COLOR, object.useDarkAppearance());

    BRect rect(intRect);
    BView* view = info.context().platformContext();

    view->PushState();
    be_control_look->DrawBorder(view, rect, view->Bounds(), base, B_PLAIN_BORDER);
    rect.InsetBy(1, 1);

    view->SetHighColor(base);
    view->FillRect(rect);

    if (position > 0) {
        BRect barRect = rect;
        barRect.right = barRect.left + barRect.Width() * position;
        view->SetHighColor(barColor);
        view->FillRect(barRect);
    }
    view->PopState();

    return false;
}

bool RenderThemeHaiku::paintCapsLockIndicator(const RenderElement&, const PaintInfo&, const FloatRect&)
{
    // Not implemented visually on Haiku usually
    return true;
}

bool RenderThemeHaiku::paintSearchFieldCancelButton(const RenderElement&, const PaintInfo& info, const FloatRect& intRect)
{
    if (!be_control_look)
        return true;

    // Draw a simple 'x' or similar?
    // Or simpler: just let WebCore handle it or draw standard button?
    // Let's draw a small X in a circle.

    BView* view = info.context().platformContext();
    view->PushState();

    // TODO: Use BControlLook if possible, but there isn't a standard cancel button there.
    // For now simple drawing.
    BRect rect(intRect);
    view->SetHighColor(ui_color(B_CONTROL_TEXT_COLOR));
    view->SetPenSize(2);

    rect.InsetBy(2, 2);
    view->StrokeLine(rect.LeftTop(), rect.RightBottom());
    view->StrokeLine(rect.LeftBottom(), rect.RightTop());

    view->PopState();
    return false;
}

bool RenderThemeHaiku::paintSearchFieldResultsDecoration(const RenderElement&, const PaintInfo& info, const FloatRect& intRect)
{
    if (!be_control_look)
        return true;

    // Magnifier glass
    BView* view = info.context().platformContext();
    view->PushState();
    BRect rect(intRect);
    view->SetHighColor(ui_color(B_CONTROL_TEXT_COLOR));
    view->SetPenSize(2);

    // Simple circle and handle
    float size = std::min(rect.Width(), rect.Height());
    BPoint center = rect.Center();
    view->StrokeEllipse(center, size/3, size/3);
    BPoint start = center;
    start.x += size/3 * 0.7;
    start.y += size/3 * 0.7;
    BPoint end = center;
    end.x += size/2;
    end.y += size/2;
    view->StrokeLine(start, end);

    view->PopState();
    return false;
}

void RenderThemeHaiku::adjustSearchFieldStyle(RenderStyle& style, const Element* element) const
{
    adjustTextFieldStyle(style, element);
    style.setBoxShadow(CSS::Keyword::None { });
}

void RenderThemeHaiku::adjustSearchFieldCancelButtonStyle(RenderStyle& style, const Element*) const
{
    style.resetBorder();
    style.resetBorderRadius();
    style.setPadding(WebCore::Style::PaddingEdge::Fixed { 0 }, WebCore::Style::PaddingEdge::Fixed { 0 }, WebCore::Style::PaddingEdge::Fixed { 0 }, WebCore::Style::PaddingEdge::Fixed { 0 });
    // Keep it square
    // style.setWidth...
}

void RenderThemeHaiku::adjustSearchFieldDecorationStyle(RenderStyle& style, const Element*) const
{
    style.resetBorder();
    style.resetBorderRadius();
    style.setPadding(WebCore::Style::PaddingEdge::Fixed { 0 }, WebCore::Style::PaddingEdge::Fixed { 0 }, WebCore::Style::PaddingEdge::Fixed { 0 }, WebCore::Style::PaddingEdge::Fixed { 0 });
}

void RenderThemeHaiku::adjustMenuListStyle(RenderStyle& style, const Element* element) const
{
    adjustMenuListButtonStyle(style, element);
}

void RenderThemeHaiku::adjustMenuListButtonStyle(RenderStyle& style, const Element*) const
{
    style.resetBorder();
    style.resetBorderRadius();

    int labelSpacing = be_control_look ? static_cast<int>(be_control_look->DefaultLabelSpacing()) : 3;
    // Position the text correctly within the select box and make the box wide enough to fit the dropdown button
    style.setPaddingTop(WebCore::Style::PaddingEdge::Fixed { 3 } );
    style.setPaddingLeft(WebCore::Style::PaddingEdge::Fixed { 3 + labelSpacing });
    style.setPaddingRight(WebCore::Style::PaddingEdge::Fixed { 22 });
    style.setPaddingBottom(WebCore::Style::PaddingEdge::Fixed {3 });

    // Height is locked to auto
    style.setHeight(CSS::Keyword::Auto { });

    // Calculate our min-height
    const int menuListButtonMinHeight = 20;
    int minHeight = style.computedFontSize();
    minHeight = std::max(minHeight, menuListButtonMinHeight);

    style.setMinHeight(WebCore::Style::MinimumSize::Fixed { minHeight });
}

void RenderThemeHaiku::paintMenuListButtonDecorations(const RenderBox& object, const PaintInfo& info, const FloatRect& floatRect)
{
    if (!be_control_look)
        return;

    rgb_color base = colorForValue(B_CONTROL_BACKGROUND_COLOR, object.firstChild()->useDarkAppearance());
        // TODO get the color from PaintInfo?
    BRect rect = floatRect;
    BView* view = info.context().platformContext();
    uint32 flags = flagsForObject(dynamic_cast<RenderElement&>(*object.firstChild())) & ~BControlLook::B_CLICKED;

    view->PushState();
    be_control_look->DrawMenuFieldFrame(view, rect, view->Bounds(), base, base, flags);
    be_control_look->DrawMenuFieldBackground(view, rect, view->Bounds(), base, true, flags);
    view->PopState();
}

bool RenderThemeHaiku::paintCheckbox(const RenderElement& object, const PaintInfo& info, const FloatRect& zoomedRect)
{
    if (!be_control_look)
        return true;

    rgb_color base = colorForValue(B_CONTROL_BACKGROUND_COLOR, object.useDarkAppearance());
        // TODO get the color from PaintInfo?
    BRect rect(zoomedRect);
    BView* view = info.context().platformContext();
    uint32 flags = flagsForObject(object) & ~BControlLook::B_CLICKED;

    be_control_look->DrawCheckBox(view, rect, view->Bounds(), base, flags);
    return false;
}

bool RenderThemeHaiku::paintRadio(const RenderElement& object, const PaintInfo& info, const FloatRect& zoomedRect)
{
    if (!be_control_look)
        return true;

    rgb_color base = colorForValue(B_CONTROL_BACKGROUND_COLOR, object.useDarkAppearance());
        // TODO get the color from PaintInfo?
    BRect rect(zoomedRect);
    BView* view = info.context().platformContext();
    uint32 flags = flagsForObject(object) & ~BControlLook::B_CLICKED;

    be_control_look->DrawRadioButton(view, rect, view->Bounds(), base, flags);
    return false;
}

bool RenderThemeHaiku::paintButton(const RenderElement& object, const PaintInfo& info, const FloatRect& zoomedRect)
{
    if (!be_control_look)
        return true;

    rgb_color base = colorForValue(B_CONTROL_BACKGROUND_COLOR, object.useDarkAppearance());
        // TODO get the color from PaintInfo?
    BRect rect(zoomedRect);
    BView* view = info.context().platformContext();
    uint32 flags = flagsForObject(object);

    be_control_look->DrawButtonFrame(view, rect, view->Bounds(), base, view->ViewColor(), flags);
    be_control_look->DrawButtonBackground(view, rect, view->Bounds(), base, flags);

    return false;
}

bool RenderThemeHaiku::paintProgressBar(const RenderElement& object, const PaintInfo& info, const FloatRect& intRect)
{
    if (!be_control_look)
        return true;

    if (!is<RenderProgress>(object))
        return true;

    const auto& renderProgress = downcast<RenderProgress>(object);
    double position = renderProgress.position();

    rgb_color base = colorForValue(B_CONTROL_BACKGROUND_COLOR, object.useDarkAppearance());
    rgb_color barColor = colorForValue(B_CONTROL_HIGHLIGHT_COLOR, object.useDarkAppearance());

    BRect rect(intRect);
    BView* view = info.context().platformContext();

    view->PushState();
    be_control_look->DrawBorder(view, rect, view->Bounds(), base, B_PLAIN_BORDER);
    rect.InsetBy(1, 1);

    view->SetHighColor(base);
    view->FillRect(rect);

    if (position > 0) {
        BRect barRect = rect;
        barRect.right = barRect.left + barRect.Width() * position;
        view->SetHighColor(barColor);
        view->FillRect(barRect);
    }
    view->PopState();

    return false;
}

bool RenderThemeHaiku::paintSearchField(const RenderElement& object, const PaintInfo& info, const FloatRect& intRect)
{
    return paintTextField(object, info, intRect);
}

bool RenderThemeHaiku::paintInnerSpinButton(const RenderElement& object, const PaintInfo& info, const FloatRect& intRect)
{
    if (!be_control_look)
        return true;

    rgb_color base = colorForValue(B_CONTROL_BACKGROUND_COLOR, object.useDarkAppearance());
    BRect rect(intRect);
    BView* view = info.context().platformContext();
    uint32 flags = flagsForObject(object);

    BRect topRect = rect;
    topRect.bottom = topRect.top + topRect.Height() / 2;
    BRect bottomRect = rect;
    bottomRect.top = topRect.bottom + 1;

    view->PushState();
    be_control_look->DrawButtonFrame(view, topRect, view->Bounds(), base, view->ViewColor(), flags);
    be_control_look->DrawButtonBackground(view, topRect, view->Bounds(), base, flags);
    be_control_look->DrawArrowShape(view, topRect, view->Bounds(), base, BControlLook::B_UP_ARROW, flags, B_DARKEN_MAX_TINT);

    be_control_look->DrawButtonFrame(view, bottomRect, view->Bounds(), base, view->ViewColor(), flags);
    be_control_look->DrawButtonBackground(view, bottomRect, view->Bounds(), base, flags);
    be_control_look->DrawArrowShape(view, bottomRect, view->Bounds(), base, BControlLook::B_DOWN_ARROW, flags, B_DARKEN_MAX_TINT);
    view->PopState();

    return false;
}

Style::PreferredSizePair RenderThemeHaiku::controlSize(StyleAppearance appearance,
    const FontCascade& font, const Style::PreferredSizePair& minimum, float zoom) const
{
    switch (appearance) {
        case StyleAppearance::Checkbox:
        case StyleAppearance::Radio:
        {
            // Keep in sync with min size code in BCheckbox constructor
            float minHeight = (float)ceil(6.0f + font.size());
            return {
                Style::PreferredSize::Fixed { minHeight },
                Style::PreferredSize::Fixed { minHeight }
            };
        }

        default:
            return RenderTheme::controlSize(appearance, font, minimum, zoom);
    }
}

uint32 RenderThemeHaiku::flagsForObject(const RenderElement& object) const
{
    uint32 flags = BControlLook::B_BLEND_FRAME;
    if (!isEnabled(object))
        flags |= BControlLook::B_DISABLED;
    if (isFocused(object))
        flags |= BControlLook::B_FOCUSED;
    if (isPressed(object))
        flags |= BControlLook::B_CLICKED;
    if (isChecked(object))
        flags |= BControlLook::B_ACTIVATED;
    if (isHovered(object))
        flags |= BControlLook::B_HOVER;
    return flags;
}


rgb_color RenderThemeHaiku::colorForValue(color_which colorConstant, bool useDarkAppearance) const
{
    rgb_color systemColor = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
    if (useDarkAppearance) {
        if (systemColor.Brightness() > 127) // system is in light mode, but we need a dark color
            return BPrivate::GetSystemColor(colorConstant, true);
    } else {
        if (systemColor.Brightness() < 127) // system is in dark mode but we need a light color
            return BPrivate::GetSystemColor(colorConstant, false);
    }
    return ui_color(colorConstant);
}


String RenderThemeHaiku::mediaControlsBase64StringForIconNameAndType(const String& iconName, const String& iconType)
{
    // FIXME: Load icon from resources
    return { };
}

String RenderThemeHaiku::mediaControlsFormattedStringForDuration(double durationInSeconds)
{
    // FIXME: Format this somehow, maybe through BDateTime?
    return makeString(durationInSeconds);
}


Color RenderThemeHaiku::systemColor(CSSValueID cssValueID, OptionSet<StyleColorOptions> options) const
{
    const bool useDarkAppearance = options.contains(StyleColorOptions::UseDarkAppearance);

    switch (cssValueID) {
    case CSSValueButtonface:
        return colorForValue(B_CONTROL_BACKGROUND_COLOR, useDarkAppearance);

    // Doesn't exist?
    //case CSSValueButtonborder:
    //    return colorForValue(B_CONTROL_BORDER_COLOR, useDarkAppearence);

    case CSSValueActivebuttontext:
    case CSSValueButtontext:
        return colorForValue(B_CONTROL_TEXT_COLOR, useDarkAppearance);

    case CSSValueField:
    case CSSValueCanvas:
    case CSSValueWindow:
        return colorForValue(B_DOCUMENT_BACKGROUND_COLOR, useDarkAppearance);

    case CSSValueCanvastext:
    case CSSValueFieldtext:
        return colorForValue(B_DOCUMENT_TEXT_COLOR, useDarkAppearance);

    case CSSValueWebkitFocusRingColor:
    case CSSValueActiveborder:
    case CSSValueHighlight:
        return colorForValue(B_CONTROL_HIGHLIGHT_COLOR, useDarkAppearance);

    case CSSValueHighlighttext:
        return colorForValue(B_CONTROL_TEXT_COLOR, useDarkAppearance);

    case CSSValueWebkitLink:
    case CSSValueLinktext:
        return colorForValue(B_LINK_TEXT_COLOR, useDarkAppearance);

    case CSSValueVisitedtext:
        return colorForValue(B_LINK_VISITED_COLOR, useDarkAppearance);

    // case CSSValueWebkitActivetext:
    case CSSValueWebkitActivelink:
        return colorForValue(B_LINK_ACTIVE_COLOR, useDarkAppearance);

    /* is there any haiku colors that make sense to use here?
    case CSSValueSelecteditem:
    case CSSValueSelecteditemtext:
    case CSSValueMark:
    case CSSValueMarkText:
    */
    default:
        return RenderTheme::systemColor(cssValueID, options);
    }
}

} // namespace WebCore
