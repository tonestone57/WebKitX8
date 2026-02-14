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
#include "PlatformWheelEvent.h"

#include <InterfaceDefs.h>
#include <Message.h>
#include <View.h>
#include <wtf/MonotonicTime.h>

namespace WebCore {

PlatformWheelEvent::PlatformWheelEvent(const BMessage* message)
    : PlatformEvent(PlatformEvent::Type::Wheel)
    , m_granularity(PlatformWheelEventGranularity::ScrollByPixelWheelEvent)
{
    float deltaX = 0;
    float deltaY = 0;
    message->FindFloat("be:wheel_delta_x", &deltaX);
    message->FindFloat("be:wheel_delta_y", &deltaY);

    // Invert direction to match WebKit expectations (usually negative delta is up/left)
    // Haiku delta: positive is down/right.
    // WebKit delta: positive is Up/Left usually? Wait.
    // On Mac: deltaY > 0 is scrolling UP (content moves down).
    // On Windows: delta > 0 is scrolling UP.
    // Haiku: deltaY > 0 is scrolling DOWN (content moves up).
    // So we need to invert?
    // Let's assume standard behavior:
    // WebKit ScrollView::wheelEvent:
    // deltaX/Y are passed to handleWheelEvent.
    // Usually deltaY > 0 means scroll up.

    m_deltaX = -deltaX;
    m_deltaY = -deltaY;

    m_wheelTicksX = m_deltaX;
    m_wheelTicksY = m_deltaY;

    // Scale by some factor? Usually browsers expect pixels.
    // Haiku deltas are "ticks".
    // 1 tick = ~40 pixels.
    const float kStep = 40.0f;
    m_deltaX *= kStep;
    m_deltaY *= kStep;

    BPoint where;
    if (message->FindPoint("be:view_where", &where) == B_OK)
        m_position = where;

    BPoint screenWhere;
    if (message->FindPoint("screen_where", &screenWhere) == B_OK)
        m_globalPosition = screenWhere;

    int32 modifiers = message->FindInt32("modifiers");
    if (modifiers & B_SHIFT_KEY)
        m_modifiers.add(PlatformEvent::Modifier::ShiftKey);
    if (modifiers & B_COMMAND_KEY)
        m_modifiers.add(PlatformEvent::Modifier::ControlKey);
    if (modifiers & B_CONTROL_KEY)
        m_modifiers.add(PlatformEvent::Modifier::AltKey);
    if (modifiers & B_OPTION_KEY)
        m_modifiers.add(PlatformEvent::Modifier::MetaKey);

    int64 when;
    if (message->FindInt64("when", &when) == B_OK)
         m_timestamp = MonotonicTime::fromRawSeconds(when / 1000000.0);
    else
         m_timestamp = MonotonicTime::now();
}

} // namespace WebCore
