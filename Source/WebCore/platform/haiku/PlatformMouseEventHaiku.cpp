/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
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
#include "PlatformMouseEvent.h"

#include <Message.h>
#include <View.h>
#include <wtf/MonotonicTime.h>

namespace WebCore {

PlatformMouseEvent::PlatformMouseEvent(const BMessage* message)
    : PlatformEvent(PlatformEvent::Type::MouseMoved)
{
    BPoint where(0, 0);
    if (message->FindPoint("be:view_where", &where) == B_OK)
        m_position = where;

    BPoint screenWhere(0, 0);
    if (message->FindPoint("screen_where", &screenWhere) == B_OK)
        m_globalPosition = screenWhere;

    if (message->FindInt32("clicks", &m_clickCount) != B_OK)
        m_clickCount = 0;

    int64 when;
    if (message->FindInt64("when", &when) == B_OK)
        m_timestamp = MonotonicTime::fromRawSeconds(when / 1000000.0);
    else
        m_timestamp = MonotonicTime::now();

    int32 buttons = 0; // Current buttons state
    int32 activeButtons = 0; // For m_buttons
    int32 changedButton = 0; // For m_button

    message->FindInt32("buttons", &buttons);
    activeButtons = buttons;

    if (message->what == B_MOUSE_UP) {
        int32 lastButtons = 0;
        if (message->FindInt32("webkit:last_buttons", &lastButtons) == B_OK) {
            changedButton = lastButtons ^ buttons;
        } else {
            // Fallback: Assume left button if we can't determine
            changedButton = B_PRIMARY_MOUSE_BUTTON;
        }
    } else {
        // For Down and Moved, 'buttons' represents the current state.
        // For Down, the changed button is the one added.
        // If we have history, we could compute it, but usually 'buttons' is enough
        // if only one button is pressed. If chording, we might need last_buttons too.
        int32 lastButtons = 0;
        if (message->what == B_MOUSE_DOWN && message->FindInt32("webkit:last_buttons", &lastButtons) == B_OK) {
            changedButton = lastButtons ^ buttons;
        } else {
            changedButton = buttons;
        }
    }

    auto mapButton = [](int32 b) {
        if (b & B_PRIMARY_MOUSE_BUTTON) return MouseButton::Left;
        if (b & B_SECONDARY_MOUSE_BUTTON) return MouseButton::Right;
        if (b & B_TERTIARY_MOUSE_BUTTON) return MouseButton::Middle;
        return MouseButton::None;
    };

    m_button = mapButton(changedButton);

    if (activeButtons & B_PRIMARY_MOUSE_BUTTON)
        m_buttons |= 1; // Left
    if (activeButtons & B_SECONDARY_MOUSE_BUTTON)
        m_buttons |= 2; // Right
    if (activeButtons & B_TERTIARY_MOUSE_BUTTON)
        m_buttons |= 4; // Middle

    switch (message->what) {
    case B_MOUSE_DOWN:
        m_type = PlatformEvent::Type::MousePressed;
        break;
    case B_MOUSE_UP:
        m_type = PlatformEvent::Type::MouseReleased;
        break;
    case B_MOUSE_MOVED:
    default:
        m_type = PlatformEvent::Type::MouseMoved;
        m_button = MouseButton::None; // Explicitly None for move
        break;
    };

    int32 modifiers = message->FindInt32("modifiers");
    if (modifiers & B_SHIFT_KEY)
        m_modifiers.add(PlatformEvent::Modifier::ShiftKey);
    if (modifiers & B_COMMAND_KEY)
        m_modifiers.add(PlatformEvent::Modifier::ControlKey);
    if (modifiers & B_CONTROL_KEY)
        m_modifiers.add(PlatformEvent::Modifier::AltKey);
    if (modifiers & B_OPTION_KEY)
        m_modifiers.add(PlatformEvent::Modifier::MetaKey);
}

} // namespace WebCore
