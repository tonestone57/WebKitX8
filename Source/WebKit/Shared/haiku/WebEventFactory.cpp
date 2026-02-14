/*
 * Copyright (C) 2019, 2024 Haiku Inc. All rights reserved.
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
#include "WebEventFactory.h"

#include "WebEventModifier.h"
#include "WebMouseEventButton.h"
#include "WebEventType.h"
#include "WebPlatformTouchPoint.h"
#include "NativeWebTouchEvent.h"

#include "WebCore/PlatformKeyboardEvent.h"
#include <WebCore/IntPoint.h>
#include <wtf/WallTime.h>

#include <AppDefs.h>
#include <InterfaceDefs.h>
#include <Message.h>
#include <View.h>

namespace WebKit {
using namespace WebCore;

int32_t WebEventFactory::currentMouseButtons = 0;
WebMouseEventButton WebEventFactory::currentMouseButton = WebMouseEventButton::None;

WebMouseEvent WebEventFactory::createWebMouseEvent(const BMessage* message)
{
    WebEventType type;
    switch (message->what) {
    case B_MOUSE_DOWN:
        type = WebEventType::MouseDown;
        break;
    case B_MOUSE_UP:
        type = WebEventType::MouseUp;
        break;
    case B_MOUSE_MOVED:
        type = WebEventType::MouseMove;
        break;
    default:
        // Should not happen, but safe fallback
        type = WebEventType::MouseMove;
        break;
    }

    int32_t previousMouseButtons = currentMouseButtons;
    message->FindInt32("buttons", &currentMouseButtons);

    WebMouseEventButton button = WebMouseEventButton::None;
    if (type != WebEventType::MouseMove) {
        int32_t changedButtons = previousMouseButtons ^ currentMouseButtons;
        if (changedButtons & B_TERTIARY_MOUSE_BUTTON)
            button = WebMouseEventButton::Middle;
        else if (changedButtons & B_SECONDARY_MOUSE_BUTTON)
            button = WebMouseEventButton::Right;
        else if (changedButtons & B_PRIMARY_MOUSE_BUTTON)
            button = WebMouseEventButton::Left;

        if (type == WebEventType::MouseDown)
            currentMouseButton = button;
        else if (type == WebEventType::MouseUp)
            currentMouseButton = WebMouseEventButton::None;
    } else {
        button = currentMouseButton;
    }

    OptionSet<WebEventModifier> modifiers;
    int32 nativeModifiers;
    if (message->FindInt32("modifiers", &nativeModifiers) != B_OK)
        nativeModifiers = 0;

    if (nativeModifiers & B_SHIFT_KEY)
        modifiers.add(WebEventModifier::ShiftKey);
    if (nativeModifiers & B_CONTROL_KEY)
        modifiers.add(WebEventModifier::AltKey); // Haiku Control is Alt
    if (nativeModifiers & B_COMMAND_KEY)
        modifiers.add(WebEventModifier::ControlKey); // Haiku Command is Control/Meta
    if (nativeModifiers & B_OPTION_KEY)
        modifiers.add(WebEventModifier::MetaKey);
    if (nativeModifiers & B_CAPS_LOCK)
        modifiers.add(WebEventModifier::CapsLockKey);

    BPoint globalPosition;
    message->FindPoint("screen_where", &globalPosition);

    BPoint viewPosition;
    message->FindPoint("be:view_where", &viewPosition);

    int32 clickCount;
    if (message->FindInt32("clicks", &clickCount) != B_OK)
        clickCount = 0;

    int32 deltaX = 0;
    int32 deltaY = 0;
    message->FindInt32("be:delta_x", &deltaX);
    message->FindInt32("be:delta_y", &deltaY);

    int64 when;
    MonotonicTime timestamp = MonotonicTime::now();
    if (message->FindInt64("when", &when) == B_OK)
        timestamp = MonotonicTime::fromRawSeconds(when / 1000000.0);

    return WebMouseEvent(
        WebEvent { type, modifiers, timestamp },
        button,
        static_cast<unsigned short>(currentMouseButtons), // simplistic mapping
        IntPoint(viewPosition),
        IntPoint(globalPosition),
        static_cast<float>(deltaX),
        static_cast<float>(deltaY),
        0.0f, // deltaZ
        clickCount
    );
}


WebKeyboardEvent WebEventFactory::createWebKeyboardEvent(const BMessage* message)
{
    WebEventType type;
    auto messageType = message->what;
    if (messageType == B_KEY_DOWN || messageType == B_UNMAPPED_KEY_DOWN)
        type = WebEventType::KeyDown;
    else if (messageType == B_KEY_UP || messageType == B_UNMAPPED_KEY_UP)
        type = WebEventType::KeyUp;
    else
        return WebKeyboardEvent(WebEvent { WebEventType::KeyDown, { }, MonotonicTime::now() }, String(), String(), String(), String(), String(), 0, 0, 0, false, false, false);

    int32 nativeVirtualKeyCode = 0;
    int32 nativeModifiers = 0;
    int32 rawChar = 0;
    bool autorepeat = false;
    const char* bytes = "";

    message->FindInt32("key", &nativeVirtualKeyCode);
    message->FindInt32("modifiers", &nativeModifiers);
    message->FindBool("be:key_repeat", &autorepeat);
    if (message->FindInt32("raw_char", &rawChar) != B_OK)
        rawChar = 0;
    if (message->FindString("bytes", &bytes) != B_OK)
        bytes = "";

    OptionSet<WebEventModifier> modifiers;
    if (nativeModifiers & B_SHIFT_KEY)
        modifiers.add(WebEventModifier::ShiftKey);
    if (nativeModifiers & B_COMMAND_KEY)
        modifiers.add(WebEventModifier::ControlKey); // Haiku Command -> Control
    if (nativeModifiers & B_CONTROL_KEY)
        modifiers.add(WebEventModifier::AltKey);     // Haiku Control -> Alt
    if (nativeModifiers & B_OPTION_KEY)
        modifiers.add(WebEventModifier::MetaKey);
    if (nativeModifiers & B_CAPS_LOCK)
        modifiers.add(WebEventModifier::CapsLockKey);

    BString bbytes(bytes);

    int64 when;
    MonotonicTime timestamp = MonotonicTime::now();
    if (message->FindInt64("when", &when) == B_OK)
        timestamp = MonotonicTime::fromRawSeconds(when / 1000000.0);

    return WebKeyboardEvent(
        WebEvent{ type, modifiers, timestamp },
        String::fromUTF8(bytes), // text
        String::fromUTF8(bytes), // unmodifiedText
        PlatformKeyboardEvent::KeyValueForKeyEvent(bbytes, nativeVirtualKeyCode), // key
        PlatformKeyboardEvent::KeyCodeForKeyEvent(nativeVirtualKeyCode), // code
        PlatformKeyboardEvent::keyIdentifierForHaikuKeyCode(bytes[0], nativeVirtualKeyCode), // Keyidentifier
        PlatformKeyboardEvent::windowsKeyCodeForKeyEvent(bytes[0], nativeVirtualKeyCode), // windowsVirtualKeyCode
        nativeVirtualKeyCode, // nativeVirtualKeyCode
        0, // macCharCode
        autorepeat,
        false, // isKeypad
        false); // isSystemKey
}

WebWheelEvent WebEventFactory::createWebWheelEvent(const BMessage* message)
{
    OptionSet<WebEventModifier> modifiers;
    int32 nativeModifiers;
    if (message->FindInt32("modifiers", &nativeModifiers) == B_OK) {
        if (nativeModifiers & B_SHIFT_KEY)
            modifiers.add(WebEventModifier::ShiftKey);
        if (nativeModifiers & B_COMMAND_KEY)
            modifiers.add(WebEventModifier::ControlKey);
        if (nativeModifiers & B_CONTROL_KEY)
            modifiers.add(WebEventModifier::AltKey);
        if (nativeModifiers & B_OPTION_KEY)
            modifiers.add(WebEventModifier::MetaKey);
        if (nativeModifiers & B_CAPS_LOCK)
            modifiers.add(WebEventModifier::CapsLockKey);
    }

    float wheelDeltaX = 0;
    float wheelDeltaY = 0;

    message->FindFloat("be:wheel_delta_x", &wheelDeltaX);
    message->FindFloat("be:wheel_delta_y", &wheelDeltaY);

    // Invert to match standard web behavior (up/left is negative)?
    // Wait, usually WheelEvent deltaY positive means scrolling DOWN.
    // Haiku deltaY positive means scrolling DOWN (content moves up).
    // So Haiku matches.
    // But `PlatformWheelEventHaiku` inverted it.
    // WebWheelEvent should probably pass raw deltas or let PlatformWheelEvent handle it.
    // Actually WebWheelEvent takes deltaX/Y.
    // Let's invert here too to match PlatformWheelEventHaiku logic if we want consistent behavior
    // or keep it raw if WebProcess handles it.
    // Let's invert because standard is: scroll DOWN -> deltaY positive?
    // Wait, DOM WheelEvent deltaY > 0 is scrolling DOWN.
    // Haiku scrollbar value increases when scrolling DOWN.
    // Haiku wheel delta > 0 is scrolling DOWN.
    // So Haiku matches DOM.
    // Why did I invert in PlatformWheelEventHaiku?
    // Maybe PlatformWheelEvent expects different sign?
    // "On Windows, a positive delta corresponds to scrolling up." (MSDN)
    // "On Mac, a positive delta corresponds to scrolling up." (Cocoa event)
    // So yes, usually platform events are inverted relative to DOM.
    // So I should invert here too.

    wheelDeltaX = -wheelDeltaX;
    wheelDeltaY = -wheelDeltaY;

    // Scale
    const float kStep = 40.0f;
    wheelDeltaX *= kStep;
    wheelDeltaY *= kStep;

    int64 when;
    MonotonicTime timestamp = MonotonicTime::now();
    if (message->FindInt64("when", &when) == B_OK)
        timestamp = MonotonicTime::fromRawSeconds(when / 1000000.0);

    return WebWheelEvent(
        WebEvent{ WebEventType::Wheel, modifiers, timestamp},
        IntPoint(0, 0), // position
        IntPoint(0, 0), // globalPosition
        FloatSize(wheelDeltaX, wheelDeltaY), //delta
        FloatSize(0,0),// wheelticks
        WebWheelEvent::Granularity::ScrollByPixelWheelEvent// granularity
        );
}

WebTouchEvent WebEventFactory::createWebTouchEvent(const BMessage* message)
{
    WebEventType type;
    switch (message->what) {
        case B_TOUCH_DOWN: type = WebEventType::TouchStart; break;
        case B_TOUCH_UP: type = WebEventType::TouchEnd; break;
        case B_TOUCH_MOVED: type = WebEventType::TouchMove; break;
        case B_TOUCH_CANCEL: type = WebEventType::TouchCancel; break;
        default: type = WebEventType::TouchCancel; break;
    }

    OptionSet<WebEventModifier> modifiers;
    int32 nativeModifiers;
    if (message->FindInt32("modifiers", &nativeModifiers) == B_OK) {
        if (nativeModifiers & B_SHIFT_KEY) modifiers.add(WebEventModifier::ShiftKey);
        if (nativeModifiers & B_COMMAND_KEY) modifiers.add(WebEventModifier::ControlKey);
        if (nativeModifiers & B_CONTROL_KEY) modifiers.add(WebEventModifier::AltKey);
        if (nativeModifiers & B_OPTION_KEY) modifiers.add(WebEventModifier::MetaKey);
        if (nativeModifiers & B_CAPS_LOCK) modifiers.add(WebEventModifier::CapsLockKey);
    }

    int64 when;
    MonotonicTime timestamp = MonotonicTime::now();
    if (message->FindInt64("when", &when) == B_OK)
        timestamp = MonotonicTime::fromRawSeconds(when / 1000000.0);

    Vector<WebPlatformTouchPoint> touchPoints;

    int32 touchId;
    for (int32 i = 0; message->FindInt32("be:touch_id", i, &touchId) == B_OK; i++) {
        BPoint location;
        if (message->FindPoint("be:view_where", i, &location) != B_OK)
            location = BPoint(0,0);

        BPoint screenLocation;
        if (message->FindPoint("be:screen_where", i, &screenLocation) != B_OK)
             screenLocation = BPoint(0,0);

        WebPlatformTouchPoint::TouchPointState state = WebPlatformTouchPoint::TouchPointState::Stationary;
        switch (type) {
            case WebEventType::TouchStart: state = WebPlatformTouchPoint::TouchPointState::Pressed; break;
            case WebEventType::TouchEnd: state = WebPlatformTouchPoint::TouchPointState::Released; break;
            case WebEventType::TouchMove: state = WebPlatformTouchPoint::TouchPointState::Moved; break;
            case WebEventType::TouchCancel: state = WebPlatformTouchPoint::TouchPointState::Cancelled; break;
            default: break;
        }

        touchPoints.append(WebPlatformTouchPoint(touchId, state, IntPoint(screenLocation), IntPoint(location)));
    }

    return WebTouchEvent(WebEvent { type, modifiers, timestamp }, WTFMove(touchPoints), { }, { });
}

}
