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
#include <WebCore/Color.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>

#include <ColorControl.h>
#include <Window.h>

namespace WebKit {
using namespace WebCore;

class ColorPickerWindow : public BWindow {
public:
    ColorPickerWindow(BRect frame, const char* title, WebColorPickerHaiku* picker)
        : BWindow(frame, title, B_FLOATING_WINDOW, B_NOT_RESIZABLE | B_NOT_ZOOMABLE)
        , m_picker(picker)
    {
    }

    void MessageReceived(BMessage* message) override
    {
        switch (message->what) {
        case 'colr': {
            rgb_color color;
            if (message->FindColor("color", &color) == B_OK)
                m_picker->colorChanged(WebCore::Color(color.red, color.green, color.blue, 255));
            break;
        }
        default:
            BWindow::MessageReceived(message);
        }
    }

    bool QuitRequested() override
    {
        if (m_picker)
            m_picker->endPicker();
        return true;
    }

private:
    WebColorPickerHaiku* m_picker;
};

Ref<WebColorPickerHaiku> WebColorPickerHaiku::create(WebPageProxy& page, const Color& initialColor, const IntRect& rect)
{
    return adoptRef(*new WebColorPickerHaiku(page, initialColor, rect));
}

WebColorPickerHaiku::WebColorPickerHaiku(WebPageProxy& page, const Color& initialColor, const IntRect&)
    : WebColorPicker(&page.colorPickerClient())
    , m_window(nullptr)
    , m_picker(nullptr)
{
    showColorPicker(initialColor);
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
    WebColorPicker::endPicker();
    m_window = nullptr; // Window deletes itself on Quit
}

void WebColorPickerHaiku::colorChanged(const Color& color)
{
    if (client())
        client()->didChooseColor(color);
}

void WebColorPickerHaiku::setSelectedColor(const Color& color)
{
    if (m_picker && m_window && m_window->Lock()) {
        m_picker->SetValue(color.toSRGBLossy<uint8_t>());
        m_window->Unlock();
    }
}

void WebColorPickerHaiku::showColorPicker(const Color& initialColor)
{
    if (m_window)
        return;

    BRect frame(100, 100, 400, 300);
    m_window = new ColorPickerWindow(frame, "Color Picker", this);

    BView* bg = new BView(frame, "bg", B_FOLLOW_ALL, 0);
    bg->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
    m_window->AddChild(bg);

    m_picker = new BColorControl(BPoint(10, 10), B_CELLS_32x8, 8.0f, "colorPicker",
        new BMessage('colr'));
    m_picker->SetValue(initialColor.toSRGBLossy<uint8_t>());
    bg->AddChild(m_picker);
    m_window->ResizeTo(m_picker->Frame().Width() + 20, m_picker->Frame().Height() + 20);
    m_window->CenterOnScreen();
    m_window->Show();

    // We need to listen to messages. Since we can't easily hook into BWindow's loop from here without
    // a BHandler/Looper proxy, this is a basic "show" implementation.
    // Ideally we would set a target handler to receive 'colr' messages and call client()->didChooseColor(color).
}

} // namespace WebKit
