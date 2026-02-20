/*
 * Copyright (C) 2019 Haiku, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebColorPickerHaiku.h"

#include "WebPageProxy.h"
#include <ColorControl.h>
#include <GroupLayout.h>
#include <GroupLayoutBuilder.h>
#include <Message.h>
#include <Slider.h>
#include <WebCore/Color.h>
#include <Window.h>
#include <wtf/RunLoop.h>

namespace WebKit {
using namespace WebCore;

static rgb_color toHaikuColor(const Color& color)
{
    auto srgba = color.toSRGB<uint8_t>();
    return { srgba.red, srgba.green, srgba.blue, srgba.alpha };
}

class ColorPickerWindow : public BWindow {
public:
    ColorPickerWindow(WebColorPickerHaiku& picker, const Color& color)
        : BWindow(BRect(0, 0, 300, 200), "Color Picker", B_TITLED_WINDOW, B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS)
        , m_picker(picker)
    {
        m_colorControl = new BColorControl(B_ORIGIN, B_CELLS_32x8, 8, "picker", new BMessage('chng'));
        m_colorControl->SetValue(toHaikuColor(color));

        m_alphaSlider = new BSlider("alpha", "Alpha", new BMessage('alph'), 0, 255, B_HORIZONTAL);
        m_alphaSlider->SetModificationMessage(new BMessage('alph'));
        m_alphaSlider->SetValue(color.toSRGB<uint8_t>().alpha);

        BGroupLayout* root = new BGroupLayout(B_VERTICAL);
        SetLayout(root);

        BGroupLayoutBuilder(root)
            .Add(m_colorControl)
            .Add(m_alphaSlider)
            .AddGlue();

        CenterOnScreen();
    }

    void MessageReceived(BMessage* message) override
    {
        switch (message->what) {
        case 'chng':
        case 'alph': {
            if (!m_picker)
                return;

            rgb_color rgb = m_colorControl->ValueAsColor();
            uint8 alpha = (uint8)m_alphaSlider->Value();
            Color color(SRGBA<uint8_t> { rgb.red, rgb.green, rgb.blue, alpha });
            RunLoop::main().dispatch([picker = m_picker, color]() {
                if (picker)
                    picker->didChooseColor(color);
            });
            break;
        }
        default:
            BWindow::MessageReceived(message);
        }
    }

    bool QuitRequested() override
    {
        if (m_picker) {
            RunLoop::main().dispatch([picker = m_picker]() {
                if (picker)
                    picker->didEndChooser();
            });
        }
        return false;
    }

private:
    WeakPtr<WebColorPickerHaiku> m_picker;
    BColorControl* m_colorControl;
    BSlider* m_alphaSlider;
};

Ref<WebColorPickerHaiku> WebColorPickerHaiku::create(WebPageProxy& page, const Color& initialColor)
{
    return adoptRef(*new WebColorPickerHaiku(page, initialColor));
}

WebColorPickerHaiku::WebColorPickerHaiku(WebPageProxy& page, const Color& initialColor)
    : WebColorPicker(&page.colorPickerClient())
    , m_window(nullptr)
{
}

WebColorPickerHaiku::~WebColorPickerHaiku()
{
    if (m_window) {
        m_window->Lock();
        m_window->Quit();
    }
}

void WebColorPickerHaiku::endPicker()
{
    if (m_window) {
        auto window = m_window;
        m_window = nullptr;
        window->Lock();
        window->Quit();
    }
    WebColorPicker::endPicker();
}

void WebColorPickerHaiku::setSelectedColor(const Color& color)
{
    if (m_window && m_window->Lock()) {
        if (BView* view = m_window->FindView("picker"))
            static_cast<BColorControl*>(view)->SetValue(toHaikuColor(color));

        if (BView* view = m_window->FindView("alpha"))
            static_cast<BSlider*>(view)->SetValue(color.toSRGB<uint8_t>().alpha);

        m_window->Unlock();
    }
}

void WebColorPickerHaiku::showColorPicker(const Color& color)
{
    if (m_window)
        return;

    m_window = new ColorPickerWindow(*this, color);
    m_window->Show();
}

void WebColorPickerHaiku::didChooseColor(const Color& color)
{
    if (client())
        client()->didChooseColor(color);
}

void WebColorPickerHaiku::didEndChooser()
{
    endPicker();
}

} // namespace WebKit
