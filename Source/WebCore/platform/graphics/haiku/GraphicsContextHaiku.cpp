/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 * Copyright (C) 2015 Julian Harnath <julian.harnath@rwth-aachen.de>
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
#include "GraphicsContextHaiku.h"

#include "AffineTransform.h"
#include "Color.h"
#include "DisplayListRecorder.h"
#include "Gradient.h"
#include "ImageBuffer.h"
#include "Path.h"
#include "Pattern.h"
#include "TransformationMatrix.h"
#include "ShadowBlur.h"

#include <wtf/text/CString.h>
#include <wtf/text/TextStream.h>
#include <Bitmap.h>
#include <GradientConic.h>
#include <GradientLinear.h>
#include <GradientRadialFocus.h>
#include <GraphicsDefs.h>
#include <Picture.h>
#include <Region.h>
#include <Shape.h>
#include <Window.h>
#include <stdio.h>

namespace {

class BlendModeGuard {
    BView* m_view;
    source_alpha m_sa;
    alpha_function m_af;
public:
    BlendModeGuard(BView* view): m_view(view) {
        m_view->GetBlendingMode(&m_sa, &m_af);
    }
    
    ~BlendModeGuard() {
        m_view->SetBlendingMode(m_sa, m_af);
    }
};

static std::unique_ptr<BGradient> createGradientWithAlpha(const BGradient& original, float alpha)
{
    std::unique_ptr<BGradient> copy;
    switch (original.Type()) {
    case BGradient::TYPE_LINEAR: {
        const auto& lin = static_cast<const BGradientLinear&>(original);
        copy = std::make_unique<BGradientLinear>(lin.Start(), lin.End());
        break;
    }
    case BGradient::TYPE_RADIAL_FOCUS: {
        const auto& rad = static_cast<const BGradientRadialFocus&>(original);
        copy = std::make_unique<BGradientRadialFocus>(rad.Center(), rad.Radius(), rad.Focal());
        break;
    }
    case BGradient::TYPE_CONIC: {
        const auto& con = static_cast<const BGradientConic&>(original);
        copy = std::make_unique<BGradientConic>(con.Center(), con.Angle());
        break;
    }
    default:
        return nullptr;
    }

    for (int i = 0; i < original.CountColorStops(); ++i) {
        auto* stop = original.ColorStopAt(i);
        if (stop) {
            BGradient::ColorStop newStop = *stop;
            newStop.color.alpha = static_cast<uint8>(newStop.color.alpha * alpha);
            copy->AddColorStop(newStop, i);
        }
    }
    return copy;
}

}

namespace WebCore {

GraphicsContextHaiku::GraphicsContextHaiku(BView* view, RefPtr<BitmapRef> bitmap)
    : GraphicsContext(IsDeferred::No, {
        GraphicsContextState::Change::StrokeThickness,
        GraphicsContextState::Change::StrokeBrush,
        GraphicsContextState::Change::Alpha,
        GraphicsContextState::Change::StrokeStyle,
        GraphicsContextState::Change::FillBrush,
        GraphicsContextState::Change::FillRule,
        GraphicsContextState::Change::CompositeMode,
        GraphicsContextState::Change::ShouldAntialias,
        GraphicsContextState::Change::ImageInterpolationQuality,
    })
    , m_bitmap(bitmap)
    , m_view(view)
    , m_strokeStyle(B_SOLID_HIGH)
    , m_painter(nullptr)
{
    didUpdateState(m_state);
    
    m_fillBitmap = new BBitmap(BRect(0, 0, 5, 5), B_RGBA32);
    memset(m_fillBitmap->Bits(), 0, m_fillBitmap->BitsLength());
    
    m_view->SetDrawingMode(B_OP_ALPHA);
    m_view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_COMPOSITE);
}

GraphicsContextHaiku::~GraphicsContextHaiku()
{
}

// Draws a filled rectangle with a stroked border.
void GraphicsContextHaiku::drawRect(const FloatRect& rect, float borderThickness)
{
    if (m_state.fillBrush().pattern())
        m_state.fillBrush().pattern()->fill(*this, rect);
    else if (m_state.fillBrush().gradient()) {
        const BGradient& gradient = m_state.fillBrush().gradient()->getHaikuGradient();
        if (m_state.alpha() < 0.99f) {
            if (auto alphaGradient = createGradientWithAlpha(gradient, m_state.alpha())) {
                m_view->FillRect(rect, *alphaGradient);
            } else {
                m_view->FillRect(rect, gradient);
            }
        } else {
            m_view->FillRect(rect, gradient);
        }
    } else
        m_view->FillRect(rect, B_SOLID_LOW);

    strokeRect(rect, borderThickness);
}

void GraphicsContextHaiku::drawNativeImage(NativeImage& image, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    drawBitmap(image.platformImage().get(), destRect, srcRect, options);
}

void GraphicsContextHaiku::drawBitmap(BBitmap* image, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    BlendModeGuard guard(m_view);
    setCompositeOperation(options.compositeOperator());

    uint32 flags = 0;
    InterpolationQuality quality = options.interpolationQuality();
    if (quality == InterpolationQuality::Default)
        quality = m_imageInterpolationQuality;

    if (quality > InterpolationQuality::Low)
        flags |= B_FILTER_BITMAP_BILINEAR;

    // We rely on end-of-frame synchronization or the fact that BBitmap drawing is usually synchronous.
    m_view->DrawBitmap(image, BRect(srcRect), BRect(destRect), flags);
}

// This is only used to draw borders.
// The line width is already accounted for, the points being not the center of
// the edges, but opposite corners of the rectangle containing the line.
void GraphicsContextHaiku::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    if (strokeStyle() == WebCore::StrokeStyle::NoStroke || !strokeColor().isVisible())
        return;

    BPoint start = point1;
    BPoint end = point2;
    // This test breaks for a vertical line as wide as long, but in that
    // case there's no information to tell vertical and horizontal apart.
    if (fabs(end.y - start.y - m_view->PenSize()) < 1) {
        // Horizontal line
        end.y = start.y = (end.y + start.y) / 2;
        end.x--;
    } else {
        // Vertical line
        end.x = start.x = (end.x + start.x) / 2;
        end.y--;
    }
    m_view->StrokeLine(start, end, m_strokeStyle);
}

// This method is only used to draw the little circles used in lists.
void GraphicsContextHaiku::drawEllipse(const FloatRect& rect)
{
    if (m_state.fillBrush().pattern() || m_state.fillBrush().gradient() || fillColor().isVisible()) {
        if (m_state.fillBrush().pattern()) {
            Path path;
            path.addEllipse(rect);
            fillPath(path);
        } else if (m_state.fillBrush().gradient()) {
            const BGradient& gradient = m_state.fillBrush().gradient()->getHaikuGradient();
            if (m_state.alpha() < 0.99f) {
                if (auto alphaGradient = createGradientWithAlpha(gradient, m_state.alpha()))
                    m_view->FillEllipse(rect, *alphaGradient);
                else
                    m_view->FillEllipse(rect, gradient);
            } else
                m_view->FillEllipse(rect, gradient);
        } else
            m_view->FillEllipse(rect, B_SOLID_LOW);
    }

    if (strokeStyle() != WebCore::StrokeStyle::NoStroke && strokeThickness() > 0.0f && strokeColor().isVisible()) {
        if (m_state.strokeBrush().gradient()) {
            BShape shape;
            shape.AddEllipse(rect);
            const BGradient& gradient = m_state.strokeBrush().gradient()->getHaikuGradient();
            if (m_state.alpha() < 0.99f) {
                if (auto alphaGradient = createGradientWithAlpha(gradient, m_state.alpha()))
                    m_view->StrokeShape(&shape, *alphaGradient);
                else
                    m_view->StrokeShape(&shape, gradient);
            } else
                m_view->StrokeShape(&shape, gradient);
        } else
            m_view->StrokeEllipse(rect, m_strokeStyle);
    }
}

void GraphicsContextHaiku::strokeRect(const FloatRect& rect, float width)
{
    if (strokeStyle() == WebCore::StrokeStyle::NoStroke || width <= 0.0f || !strokeColor().isVisible())
        return;

    if (hasDropShadow()) {
        const auto shadow = dropShadow();
        ShadowBlur contextShadow(*shadow, shadowsIgnoreTransforms());
        FloatRect shadowRect = rect;
        shadowRect.inflate(width / 2.0f);
        contextShadow.drawShadowLayer(getCTM(), clipBounds(), shadowRect,
            [&](GraphicsContext& shadowContext) {
                shadowContext.setStrokeColor(Color::black);
                shadowContext.setStrokeThickness(width);
                shadowContext.strokeRect(rect, width);
            },
            [&](ImageBuffer& buffer, const FloatPoint& p, const FloatSize& s) {
                this->drawImageBuffer(buffer, FloatRect(p, s), FloatRect(FloatPoint(), s), { CompositeOperator::SourceOver });
            });
    }

    if (m_state.strokeBrush().gradient()) {
        BShape shape;
        shape.AddRect(rect);
        const BGradient& gradient = m_state.strokeBrush().gradient()->getHaikuGradient();
        if (m_state.alpha() < 0.99f) {
            if (auto alphaGradient = createGradientWithAlpha(gradient, m_state.alpha()))
                m_view->StrokeShape(&shape, *alphaGradient);
            else
                m_view->StrokeShape(&shape, gradient);
        } else
            m_view->StrokeShape(&shape, gradient);
        return;
    }

    float oldSize = m_view->PenSize();
    m_view->SetPenSize(width);
    m_view->StrokeRect(rect, m_strokeStyle);
    m_view->SetPenSize(oldSize);
}

void GraphicsContextHaiku::strokePath(const Path& path)
{
    m_view->MovePenTo(B_ORIGIN);

    if (hasDropShadow()) {
        const auto shadow = dropShadow();
        ShadowBlur contextShadow(*shadow, shadowsIgnoreTransforms());
        FloatRect shadowRect = path.boundingRect();
        shadowRect.inflate(strokeThickness() / 2.0f);
        contextShadow.drawShadowLayer(getCTM(), clipBounds(), shadowRect,
            [&](GraphicsContext& shadowContext) {
                shadowContext.setStrokeColor(Color::black);
                shadowContext.setStrokeThickness(strokeThickness());
                shadowContext.strokePath(path);
            },
            [&](ImageBuffer& buffer, const FloatPoint& p, const FloatSize& s) {
                this->drawImageBuffer(buffer, FloatRect(p, s), FloatRect(FloatPoint(), s), { CompositeOperator::SourceOver });
            });
    }

    if (m_state.strokeBrush().pattern()) {
        // Fallback to solid color for now
        if (strokeColor().isVisible())
            m_view->StrokeShape(path.platformPath(), m_strokeStyle);
    } else if (m_state.strokeBrush().gradient()) {
        const BGradient& gradient = m_state.strokeBrush().gradient()->getHaikuGradient();
        if (m_state.alpha() < 0.99f) {
            if (auto alphaGradient = createGradientWithAlpha(gradient, m_state.alpha()))
                m_view->StrokeShape(path.platformPath(), *alphaGradient);
            else
                m_view->StrokeShape(path.platformPath(), gradient);
        } else
            m_view->StrokeShape(path.platformPath(), gradient);
    } else if (strokeColor().isVisible()) {
        m_view->StrokeShape(path.platformPath(), m_strokeStyle);
    }
}

void GraphicsContextHaiku::fillRect(const FloatRect& rect, const Color& color)
{
    if (hasDropShadow()) {
        const auto shadow = dropShadow();
        ShadowBlur contextShadow(*shadow, shadowsIgnoreTransforms());
        contextShadow.drawRectShadow(*this, FloatRoundedRect(rect));
    }
    
    // FillRect doesn't respect blending modes, DrawBitmap does.
    // However, if the color is opaque and we are in Copy or SourceOver mode (mostly),
    // FillRect is much faster.
    const auto [r, g, b, a] = color.toColorTypeLossy<SRGBA<uint8_t>>().resolved();

    if (a == 255 && m_view->DrawingMode() == B_OP_COPY) {
        m_view->SetHighColor(r, g, b, 255);
        m_view->FillRect(rect);
        return;
    }

    const uint32_t c = ((a << 24) | (r << 16) | (g << 8) | b);
    m_fillBitmap->Lock();
    uint32_t *bits = reinterpret_cast<uint32_t *>(m_fillBitmap->Bits());
    if(bits[0] != c) {
        std::fill(bits, bits + m_fillBitmap->BitsLength() / 4, c);
    }
    // cannot be async because bitmap might change before the draw is executed
    m_view->DrawTiledBitmap(m_fillBitmap, BRect(rect));
    m_fillBitmap->Unlock();
}

void GraphicsContextHaiku::fillRect(const FloatRect& rect, RequiresClipToRect requiresClipToRect)
{
    if (RefPtr fillGradient = this->fillGradient()) {
        fillRect(rect, *fillGradient, fillGradientSpaceTransform(), requiresClipToRect);
        return;
    }    
    
    if (hasDropShadow()) {
        const auto shadow = dropShadow();
        ShadowBlur contextShadow(*shadow, shadowsIgnoreTransforms());
        contextShadow.drawRectShadow(*this, FloatRoundedRect(rect));
    }

    const auto [r, g, b, a] = state().fillBrush().color().toColorTypeLossy<SRGBA<uint8_t>>().resolved();

    if (a == 255 && m_view->DrawingMode() == B_OP_COPY) {
        m_view->SetHighColor(r, g, b, 255);
        m_view->FillRect(rect);
        return;
    }

    // FillRect doesn't respect blending modes, DrawBitmap does
    const uint32_t c = ((a << 24) | (r << 16) | (g << 8) | b);
    m_fillBitmap->Lock();
    uint32_t *bits = reinterpret_cast<uint32_t *>(m_fillBitmap->Bits());
    if(bits[0] != c) {
        std::fill(bits, bits + m_fillBitmap->BitsLength() / 4, c);
    }
    // cannot be async because bitmap might change before the draw is executed
    m_view->DrawTiledBitmap(m_fillBitmap, BRect(rect));
    m_fillBitmap->Unlock();
}

void GraphicsContextHaiku::fillRect(const WebCore::FloatRect& r, WebCore::Gradient& g, const WebCore::AffineTransform&, RequiresClipToRect requiresClipToRect)
{
    if (requiresClipToRect == RequiresClipToRect::Yes) {
        m_view->ClipToRect(r);
    }
    
    const BGradient& gradient = g.getHaikuGradient();
    if (m_state.alpha() < 0.99f) {
        if (auto alphaGradient = createGradientWithAlpha(gradient, m_state.alpha()))
            m_view->FillRect(r, *alphaGradient);
        else
            m_view->FillRect(r, gradient);
    } else
        m_view->FillRect(r, gradient);
}

void GraphicsContextHaiku::fillRoundedRectImpl(const FloatRoundedRect& roundRect, const Color& color)
{
    if (!color.isVisible())
        return;

    const FloatRect& rect = roundRect.rect();
    const FloatSize& topLeft = roundRect.radii().topLeft();
    const FloatSize& topRight = roundRect.radii().topRight();
    const FloatSize& bottomLeft = roundRect.radii().bottomLeft();
    const FloatSize& bottomRight = roundRect.radii().bottomRight();

    if (hasDropShadow()) {
        const auto shadow = dropShadow();
        ShadowBlur contextShadow(*shadow, shadowsIgnoreTransforms());
        contextShadow.drawRectShadow(*this, roundRect);
    }

    BPoint points[3];
    const float kRadiusBezierScale = 1.0f - 0.5522847498f; //  1 - (sqrt(2) - 1) * 4 / 3

    BShape shape;
    shape.MoveTo(BPoint(rect.maxX() - topRight.width(), rect.y()));
    points[0].x = rect.maxX() - kRadiusBezierScale * topRight.width();
    points[0].y = rect.y();
    points[1].x = rect.maxX();
    points[1].y = rect.y() + kRadiusBezierScale * topRight.height();
    points[2].x = rect.maxX();
    points[2].y = rect.y() + topRight.height();
    shape.BezierTo(points);
    shape.LineTo(BPoint(rect.maxX(), rect.maxY() - bottomRight.height()));
    points[0].x = rect.maxX();
    points[0].y = rect.maxY() - kRadiusBezierScale * bottomRight.height();
    points[1].x = rect.maxX() - kRadiusBezierScale * bottomRight.width();
    points[1].y = rect.maxY();
    points[2].x = rect.maxX() - bottomRight.width();
    points[2].y = rect.maxY();
    shape.BezierTo(points);
    shape.LineTo(BPoint(rect.x() + bottomLeft.width(), rect.maxY()));
    points[0].x = rect.x() + kRadiusBezierScale * bottomLeft.width();
    points[0].y = rect.maxY();
    points[1].x = rect.x();
    points[1].y = rect.maxY() - kRadiusBezierScale * bottomLeft.height();
    points[2].x = rect.x();
    points[2].y = rect.maxY() - bottomLeft.height();
    shape.BezierTo(points);
    shape.LineTo(BPoint(rect.x(), rect.y() + topLeft.height()));
    points[0].x = rect.x();
    points[0].y = rect.y() + kRadiusBezierScale * topLeft.height();
    points[1].x = rect.x() + kRadiusBezierScale * topLeft.width();
    points[1].y = rect.y();
    points[2].x = rect.x() + topLeft.width();
    points[2].y = rect.y();
    shape.BezierTo(points);
    shape.Close(); // Automatically completes the shape with the top border

    m_view->MovePenTo(B_ORIGIN);
    m_view->SetHighColor(color);
    m_view->FillShape(&shape);
}

void GraphicsContextHaiku::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    Path path;
    path.addRect(rect);

    if (!roundedHoleRect.radii().isZero())
        path.addRoundedRect(roundedHoleRect);
    else
        path.addRect(roundedHoleRect.rect());

    WindRule oldFillRule = fillRule();
    Color oldFillColor = fillColor();

    setFillRule(WindRule::EvenOdd);
    setFillColor(color);

    if (hasDropShadow()) {
        const auto shadow = dropShadow();
        ASSERT(shadow);
        
        ShadowBlur contextShadow(*shadow, shadowsIgnoreTransforms());
        contextShadow.drawInsetShadow(*this, rect, roundedHoleRect);
    }

    fillPath(path);

    setFillRule(oldFillRule);
    setFillColor(oldFillColor);
}

void GraphicsContextHaiku::fillPath(const Path& path)
{
    m_view->SetFillRule(fillRule() == WindRule::NonZero ? B_NONZERO : B_EVEN_ODD);
    m_view->MovePenTo(B_ORIGIN);

    if (hasDropShadow()) {
        const auto shadow = dropShadow();
        ShadowBlur contextShadow(*shadow, shadowsIgnoreTransforms());
        contextShadow.drawShadowLayer(getCTM(), clipBounds(), path.boundingRect(),
            [&](GraphicsContext& shadowContext) {
                shadowContext.setFillColor(Color::black);
                shadowContext.fillPath(path);
            },
            [&](ImageBuffer& buffer, const FloatPoint& p, const FloatSize& s) {
                this->drawImageBuffer(buffer, FloatRect(p, s), FloatRect(FloatPoint(), s), { CompositeOperator::SourceOver });
            });
    }

    drawing_mode mode = m_view->DrawingMode();

    if (m_state.fillBrush().pattern()) {
        m_view->PushState();
        m_view->ClipToShape(path.platformPath());
        m_state.fillBrush().pattern()->fill(*this, path.boundingRect());
        m_view->PopState();
    } else if (m_state.fillBrush().gradient()) {
        m_view->SetDrawingMode(B_OP_ALPHA);
        const BGradient& gradient = m_state.fillBrush().gradient()->getHaikuGradient();
        if (m_state.alpha() < 0.99f) {
            if (auto alphaGradient = createGradientWithAlpha(gradient, m_state.alpha()))
                m_view->FillShape(path.platformPath(), *alphaGradient);
            else
                m_view->FillShape(path.platformPath(), gradient);
        } else
            m_view->FillShape(path.platformPath(), gradient);
    } else {
        if (m_view->HighColor().alpha < 255)
            m_view->SetDrawingMode(B_OP_ALPHA);

        m_view->FillShape(path.platformPath(), B_SOLID_LOW);
    }

    m_view->SetDrawingMode(mode);
}

void GraphicsContextHaiku::clip(const FloatRect& rect)
{
    m_view->ClipToRect(rect);
}

void GraphicsContextHaiku::clipPath(const Path& path, WindRule windRule)
{
    int32 fillRule = m_view->FillRule();

    m_view->SetFillRule(windRule == WindRule::EvenOdd ? B_EVEN_ODD : B_NONZERO);
    m_view->ClipToShape(path.platformPath());

    m_view->SetFillRule(fillRule);
}

void GraphicsContextHaiku::clipToImageBuffer(WebCore::ImageBuffer& imageBuffer, WebCore::FloatRect const& destRect)
{
    auto nativeImage = imageBuffer.createNativeImageReference();
    if(!nativeImage)
        return;
    
    BPicture picture;
    m_view->BeginPicture(&picture);
    BBitmap* bmp = nativeImage->platformImage().get();
    
	m_view->SetDrawingMode(B_OP_ALPHA);
	m_view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_COMPOSITE);
    m_view->SetHighColor(0,0,0,0);
    m_view->FillRect(destRect);
    m_view->DrawBitmap(bmp, destRect);
    m_view->EndPicture();
    m_view->ClipToPicture(&picture);
}

void GraphicsContextHaiku::resetClip()
{
    m_view->ClipToRect(m_view->Bounds());
}


void GraphicsContextHaiku::drawPattern(NativeImage& image, const FloatRect& destRect,
    const FloatRect& tileRect, const AffineTransform& transform,
    const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    drawBitmap(image.platformImage().get(), image.size(), destRect, tileRect, transform, phase, spacing, options);
}

void GraphicsContextHaiku::drawBitmap(BBitmap* image, const WebCore::FloatSize& size, const FloatRect& destRect,
    const FloatRect& tileRect, const AffineTransform&,
    const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions&)
{
    if (!image->IsValid()) // If the image hasn't fully loaded.
        return;

    m_view->PushState();

    clip(enclosingIntRect(destRect));
    float phaseOffsetX = destRect.x() - phase.x();
    float phaseOffsetY = destRect.y() - phase.y();
    // x mod w, y mod h
    phaseOffsetX -= std::trunc(phaseOffsetX / tileRect.width()) * tileRect.width();
    phaseOffsetY -= std::trunc(phaseOffsetY / tileRect.height()) * tileRect.height();
    m_view->DrawTiledBitmapAsync(
        image, destRect, BPoint(phaseOffsetX, phaseOffsetY));
    m_view->PopState();
}


void GraphicsContextHaiku::clipOut(const Path& path)
{
    if (path.isEmpty())
        return;

    m_view->ClipToInverseShape(path.platformPath());
}

void GraphicsContextHaiku::clipOut(const FloatRect& rect)
{
    m_view->ClipToInverseRect(rect);
}

void GraphicsContextHaiku::drawFocusRing(const Path& path, float width, const Color& color)
{
    if (width <= 0 || !color.isVisible())
        return;

    m_view->PushState();
    m_view->SetHighColor(color);
    m_view->SetPenSize(width);
    m_view->StrokeShape(path.platformPath(), B_SOLID_HIGH);
    m_view->PopState();
}

void GraphicsContextHaiku::drawFocusRing(const Vector<FloatRect>& rects, float offset, float width, const Color& color)
{
    if (width <= 0 || !color.isVisible())
        return;

    unsigned rectCount = rects.size();
    if (rectCount <= 0)
        return;

    m_view->PushState();
    m_view->SetHighColor(color);
    m_view->SetPenSize(width);

    for (unsigned i = 0; i < rectCount; ++i) {
        BRect r = rects[i];
        r.InsetBy(-offset, -offset);
        m_view->StrokeRect(r, B_SOLID_HIGH);
    }
    m_view->PopState();
}

void GraphicsContextHaiku::drawLinesForText(const FloatPoint& point,
    float thickness, const std::span<const FloatSegment> widths, bool printing,
    bool doubleUnderlines, WebCore::StrokeStyle style)
{
    if (widths.empty() || style == WebCore::StrokeStyle::NoStroke)
        return;

    Color lineColor(strokeColor());
    FloatRect bounds = computeLineBoundsAndAntialiasingModeForText(
        FloatRect(point, FloatSize(widths.end()->end, thickness)),
        printing, lineColor);
    if (bounds.isEmpty() || !strokeColor().isVisible())
        return;

    float y = bounds.center().y();

    float oldSize = m_view->PenSize();
    m_view->SetPenSize(bounds.height());

    m_view->BeginLineArray(widths.size());
    for (const auto& width: widths)
    {
        m_view->AddLine(BPoint(bounds.x() + width.begin, y),
            BPoint(bounds.x() + width.end, y), m_view->HighColor());
    }
    m_view->EndLineArray();

    m_view->SetPenSize(oldSize);
}

void GraphicsContextHaiku::drawDotsForDocumentMarker(WebCore::FloatRect const& rect,
	WebCore::DocumentMarkerLineStyle)
{
    m_view->PushState();
    m_view->SetHighColor(strokeColor());
    m_view->SetPenSize(1.0);
    m_view->SetLowColor(B_TRANSPARENT_COLOR);

    pattern p = { { 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa } };
    float y = rect.maxY();
    m_view->StrokeLine(BPoint(rect.x(), y), BPoint(rect.maxX(), y), p);

    m_view->PopState();
}

/* Used by canvas.clearRect. Must clear the given rectangle with transparent black. */
void GraphicsContextHaiku::clearRect(const FloatRect& rect)
{
    m_view->SetHighColor(0, 0, 0, 0);
    m_view->SetDrawingMode(B_OP_COPY);
    m_view->FillRect(rect);
    m_view->SetDrawingMode(B_OP_ALPHA);
}

void GraphicsContextHaiku::setLineCap(LineCap lineCap)
{
    cap_mode mode = B_BUTT_CAP;
    switch (lineCap) {
    case LineCap::Round:
        mode = B_ROUND_CAP;
        break;
    case LineCap::Square:
        mode = B_SQUARE_CAP;
        break;
    case LineCap::Butt:
    default:
        break;
    }

    m_view->SetLineMode(mode, m_view->LineJoinMode(), m_view->LineMiterLimit());
}

void GraphicsContextHaiku::setLineDash(const DashArray& dashes, float dashOffset)
{
    m_dashArray = dashes;
    m_dashOffset = dashOffset;
}

void GraphicsContextHaiku::setLineJoin(LineJoin lineJoin)
{
    join_mode mode = B_MITER_JOIN;
    switch (lineJoin) {
    case LineJoin::Round:
        mode = B_ROUND_JOIN;
        break;
    case LineJoin::Bevel:
        mode = B_BEVEL_JOIN;
        break;
    case LineJoin::Miter:
    default:
        break;
    }

    m_view->SetLineMode(m_view->LineCapMode(), mode, m_view->LineMiterLimit());
}

void GraphicsContextHaiku::setMiterLimit(float limit)
{
    m_view->SetLineMode(m_view->LineCapMode(), m_view->LineJoinMode(), limit);
}

AffineTransform GraphicsContextHaiku::getCTM(IncludeDeviceScale) const
{
    BAffineTransform t = m_view->Transform();
    AffineTransform matrix(t.sx, t.shy, t.shx, t.sy, t.tx, t.ty);
    return matrix;
}

void GraphicsContextHaiku::translate(float x, float y)
{
    if (x == 0.f && y == 0.f)
        return;

    m_view->TranslateBy(x, y);
}

void GraphicsContextHaiku::rotate(float radians)
{
    if (radians == 0.f)
        return;

    m_view->RotateBy(radians);
}

void GraphicsContextHaiku::scale(const FloatSize& size)
{
    m_view->ScaleBy(size.width(), size.height());
}

void GraphicsContextHaiku::concatCTM(const AffineTransform& transform)
{
    BAffineTransform current = m_view->Transform();
    current.Multiply(transform);
    m_view->SetTransform(current);
}

void GraphicsContextHaiku::setCTM(const AffineTransform& transform)
{
    m_view->SetTransform(transform);
}

void GraphicsContextHaiku::didUpdateState(GraphicsContextState& state)
{
    if(state.changes().isEmpty()) {
        state.didApplyChanges();
        return;
    }

    if (state.changes().contains(GraphicsContextState::Change::StrokeThickness)) {
        m_view->SetPenSize(state.strokeThickness());
    }
    if (state.changes().contains(GraphicsContextState::Change::StrokeBrush)) {
        rgb_color color = state.strokeBrush().color();
        // Alpha is applied to HighColor below if needed, but we set it here primarily
        m_view->SetHighColor(color);
    }
    if (state.changes().contains(GraphicsContextState::Change::StrokeStyle)) {
        static const pattern kDottedPattern = { { 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa } };
        static const pattern kDashedPattern = { { 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0 } };

        switch (strokeStyle()) {
			case WebCore::StrokeStyle::DoubleStroke:
			case WebCore::StrokeStyle::WavyStroke:
                // FIXME: Implement fancy strokes
                m_strokeStyle = B_SOLID_HIGH;
                break;
			case WebCore::StrokeStyle::SolidStroke:
                m_strokeStyle = B_SOLID_HIGH;
                break;
			case WebCore::StrokeStyle::DottedStroke:
                m_view->SetLowColor(B_TRANSPARENT_COLOR);
                m_strokeStyle = kDottedPattern;
                break;
			case WebCore::StrokeStyle::DashedStroke:
                m_view->SetLowColor(B_TRANSPARENT_COLOR);
                m_strokeStyle = kDashedPattern;
                break;
            case WebCore::StrokeStyle::NoStroke:
                m_strokeStyle = B_SOLID_LOW;
                break;
        }
    }
    if (state.changes().contains(GraphicsContextState::Change::FillBrush)) {
        rgb_color color = state.fillBrush().color();
        m_view->SetLowColor(color);
    }
    if (state.changes().contains(GraphicsContextState::Change::FillRule))
        m_view->SetFillRule(fillRule() == WindRule::NonZero ? B_NONZERO : B_EVEN_ODD);

    if (state.changes().contains(GraphicsContextState::Change::Alpha)) {
        rgb_color stroke = m_view->HighColor();
        rgb_color fill = m_view->LowColor();
        stroke.alpha = static_cast<uint8_t>(255 * state.alpha());
        fill.alpha = static_cast<uint8_t>(255 * state.alpha());
        m_view->SetHighColor(stroke);
        m_view->SetLowColor(fill);
    }

    if (state.changes().contains(GraphicsContextState::Change::CompositeMode)) {
        drawing_mode mode = B_OP_ALPHA;
        alpha_function blending_mode = B_ALPHA_COMPOSITE;
        switch (compositeOperation()) {
            case CompositeOperator::SourceOver:
                blending_mode = B_ALPHA_COMPOSITE_SOURCE_OVER;
                break;
            case CompositeOperator::PlusLighter:
                blending_mode = B_ALPHA_COMPOSITE_LIGHTEN;
                break;
            case CompositeOperator::Difference:
                blending_mode = B_ALPHA_COMPOSITE_DIFFERENCE;
                break;
            case CompositeOperator::PlusDarker:
                blending_mode = B_ALPHA_COMPOSITE_DARKEN;
                break;
            case CompositeOperator::Clear:
                blending_mode = B_ALPHA_COMPOSITE_CLEAR;
                break;
            case CompositeOperator::DestinationOut:
                blending_mode = B_ALPHA_COMPOSITE_DESTINATION_OUT;
                break;
            case CompositeOperator::SourceAtop:
                blending_mode = B_ALPHA_COMPOSITE_SOURCE_ATOP;
                break;
            case CompositeOperator::SourceIn:
                blending_mode = B_ALPHA_COMPOSITE_SOURCE_IN;
                break;
            case CompositeOperator::SourceOut:
                blending_mode = B_ALPHA_COMPOSITE_SOURCE_OUT;
                break;
            case CompositeOperator::DestinationOver:
                blending_mode = B_ALPHA_COMPOSITE_DESTINATION_OVER;
                break;
            case CompositeOperator::DestinationAtop:
                blending_mode = B_ALPHA_COMPOSITE_DESTINATION_ATOP;
                break;
            case CompositeOperator::DestinationIn:
                blending_mode = B_ALPHA_COMPOSITE_DESTINATION_IN;
                break;
            case CompositeOperator::XOR:
                blending_mode = B_ALPHA_COMPOSITE_XOR;
                break;
            case CompositeOperator::Copy:
                blending_mode = B_ALPHA_COMPOSITE;
                break;
            default:
                fprintf(stderr, "GraphicsContext::setCompositeOperation: Unsupported composite operation %s\n",
                        compositeOperatorName(compositeMode().operation, compositeMode().blendMode).utf8().data());
        }
        m_view->SetDrawingMode(mode);
        m_view->SetBlendingMode(B_PIXEL_ALPHA, blending_mode);
    }

    if (state.changes().contains(GraphicsContextState::Change::ShouldAntialias)) {
        if (state.shouldAntialias())
            m_view->SetFlags(m_view->Flags() | B_ANTIALIASING);
        else
            m_view->SetFlags(m_view->Flags() & ~B_ANTIALIASING);
    }

    if (state.changes().contains(GraphicsContextState::Change::ImageInterpolationQuality)) {
        m_imageInterpolationQuality = state.imageInterpolationQuality();
    }

    state.didApplyChanges();
}

#if ENABLE(3D_RENDERING) && USE(TEXTURE_MAPPER)
TransformationMatrix GraphicsContextHaiku::get3DTransform() const
{
    return getCTM().toTransformationMatrix();
}

void GraphicsContextHaiku::concat3DTransform(const TransformationMatrix& transform)
{
    concatCTM(transform.toAffineTransform());
}

void GraphicsContextHaiku::set3DTransform(const TransformationMatrix& transform)
{
    setCTM(transform.toAffineTransform());
}
#endif

void GraphicsContextHaiku::beginTransparencyLayer(float opacity)
{
    GraphicsContext::beginTransparencyLayer(opacity);
    save(GraphicsContextState::Purpose::TransparencyLayer);
    m_view->BeginLayer(static_cast<uint8>(opacity * 255.0));
}

void GraphicsContextHaiku::endTransparencyLayer()
{
    GraphicsContext::endTransparencyLayer();
    m_view->EndLayer();
    restore(GraphicsContextState::Purpose::TransparencyLayer);
}

IntRect GraphicsContextHaiku::clipBounds() const
{
    BRegion region;
    m_view->GetClippingRegion(&region);
    BRect rect = region.Frame();

    BPoint points[4];
    points[0] = rect.LeftTop();
    points[1] = rect.RightBottom();
    points[2] = rect.LeftBottom();
    points[3] = rect.RightTop();

    BAffineTransform t = m_view->TransformTo(B_VIEW_COORDINATES);
    t.ApplyInverse(points, 4);

    rect.left   = std::min({points[0].x, points[1].x, points[2].x, points[3].x});
    rect.right  = std::max({points[0].x, points[1].x, points[2].x, points[3].x});
    rect.top    = std::min({points[0].y, points[1].y, points[2].y, points[3].y});
    rect.bottom = std::max({points[0].y, points[1].y, points[2].y, points[3].y});

    return IntRect(rect);
}


void GraphicsContextHaiku::save(GraphicsContextState::Purpose)
{
    m_view->PushState();
    GraphicsContext::save();
}

void GraphicsContextHaiku::restore(GraphicsContextState::Purpose)
{
    GraphicsContext::restore();
    m_view->PopState();
}


} // namespace WebCore
