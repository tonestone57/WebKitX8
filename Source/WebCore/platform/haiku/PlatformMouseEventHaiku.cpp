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
    BPoint where;
    if (message->FindPoint("be:view_where", &where) == B_OK)
        m_position = where;

    BPoint screenWhere;
    if (message->FindPoint("screen_where", &screenWhere) == B_OK)
        m_globalPosition = screenWhere;

    if (message->FindInt32("clicks", &m_clickCount) != B_OK)
        m_clickCount = 0;

    int64 when;
    if (message->FindInt64("when", &when) == B_OK)
        m_timestamp = MonotonicTime::fromRawSeconds(when / 1000000.0);
    else
        m_timestamp = MonotonicTime::now();

    int32 buttons = 0;
    // B_MOUSE_UP usually doesn't contain "buttons" state of what is pressed NOW (which is 0),
    // but sometimes "previous buttons". But WebKit expects the button that caused the event for Up/Down.
    // For MouseMoved, it expects currently pressed buttons.

    if (message->what == B_MOUSE_UP) {
        // We might not have "previous buttons" always, relying on "buttons" being 0.
        // But we need to know WHICH button was released.
        // Haiku B_MOUSE_UP doesn't explicitly tell which button was released in a separate field,
        // but we can infer if needed or just use what we have.
        // Actually, "buttons" in B_MOUSE_UP is usually 0 if no other button is held.
        // But PlatformMouseEvent expects m_button to be the button changing state.

        // Let's check if we have tracked state or if the message has info.
        // For now, we might check "buttons" in B_MOUSE_DOWN/MOVED.

        // If we can't determine, default to Left?
        // Let's see if there is a "previous buttons" field? message->FindInt32("previous buttons", &buttons)?
        // Some Haiku versions might support it?
        // If not, we rely on the fact that B_MOUSE_UP implies a release.
    } else {
        message->FindInt32("buttons", &buttons);
    }

    // Map buttons
    if (buttons & B_PRIMARY_MOUSE_BUTTON) {
        m_button = MouseButton::Left;
        m_buttons |= 1; // Left
    } else if (buttons & B_SECONDARY_MOUSE_BUTTON) {
        m_button = MouseButton::Right;
         m_buttons |= 2; // Right
    } else if (buttons & B_TERTIARY_MOUSE_BUTTON) {
        m_button = MouseButton::Middle;
         m_buttons |= 4; // Middle
    } else {
        m_button = MouseButton::None;
    }

    switch (message->what) {
    case B_MOUSE_DOWN:
        m_type = PlatformEvent::Type::MousePressed;
        break;
    case B_MOUSE_UP:
        m_type = PlatformEvent::Type::MouseReleased;
        // Logic to try to guess which button was released if m_button is None?
        // If we are releasing, 'buttons' is 0. So m_button becomes None.
        // But PlatformMouseEvent requires m_button to be set to the button being released.
        // We'll set it to Left as fallback if None, or maybe we just leave it.
        if (m_button == MouseButton::None) m_button = MouseButton::Left;
        break;
    case B_MOUSE_MOVED:
    default:
        m_type = PlatformEvent::Type::MouseMoved;
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
