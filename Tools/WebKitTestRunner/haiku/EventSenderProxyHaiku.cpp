/*
 * Copyright (C) 2014 Haiku, inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "EventSenderProxy.h"

#include "PlatformWebView.h"
#include "TestController.h"
#include "WebViewBase.h"
#include <InterfaceDefs.h>
#include <Message.h>
#include <String.h>
#include <Window.h>
#include <wtf/MonotonicTime.h>

namespace WTR {

static uint32 modifiersForWKEventModifiers(WKEventModifiers wkModifiers)
{
    uint32 modifiers = 0;
    if (wkModifiers & kWKEventModifiersShiftKey)
        modifiers |= B_SHIFT_KEY;
    if (wkModifiers & kWKEventModifiersControlKey)
        modifiers |= B_COMMAND_KEY;
    if (wkModifiers & kWKEventModifiersAltKey)
        modifiers |= B_CONTROL_KEY;
    if (wkModifiers & kWKEventModifiersMetaKey)
        modifiers |= B_OPTION_KEY;
    if (wkModifiers & kWKEventModifiersCapsLockKey)
        modifiers |= B_CAPS_LOCK;
    return modifiers;
}

static uint32 mouseButtonForWKEventMouseButton(int button)
{
    switch (button) {
    case kWKEventMouseButtonLeftButton:
        return B_PRIMARY_MOUSE_BUTTON;
    case kWKEventMouseButtonRightButton:
        return B_SECONDARY_MOUSE_BUTTON;
    case kWKEventMouseButtonMiddleButton:
        return B_TERTIARY_MOUSE_BUTTON;
    default:
        return 0;
    }
}

void EventSenderProxy::updateClickCountForButton(int button)
{
    if (m_time - m_clickTime < 1 && m_position == m_clickPosition && button == m_clickButton) {
        ++m_clickCount;
        m_clickTime = m_time;
        return;
    }

    m_clickCount = 1;
    m_clickTime = m_time;
    m_clickPosition = m_position;
    m_clickButton = button;
}

void EventSenderProxy::mouseDown(unsigned button, WKEventModifiers wkModifiers)
{
    updateClickCountForButton(button);

    auto* view = m_testController->mainWebView()->platformView().get();
    if (!view) return;

    BMessage msg(B_MOUSE_DOWN);
    int64 when = (int64)(m_time * 1000000.0);
    msg.AddInt64("when", when);
    msg.AddInt32("buttons", mouseButtonForWKEventMouseButton(button));
    msg.AddInt32("modifiers", modifiersForWKEventModifiers(wkModifiers));
    msg.AddInt32("clicks", m_clickCount);

    BPoint where(m_position.x, m_position.y);
    msg.AddPoint("be:view_where", where);

    BPoint screenWhere = where;
    if (view->Window())
        screenWhere += view->Window()->Frame().LeftTop();
    msg.AddPoint("screen_where", screenWhere);

    view->MessageReceived(&msg);
}

void EventSenderProxy::mouseUp(unsigned button, WKEventModifiers wkModifiers)
{
    auto* view = m_testController->mainWebView()->platformView().get();
    if (!view) return;

    BMessage msg(B_MOUSE_UP);
    int64 when = (int64)(m_time * 1000000.0);
    msg.AddInt64("when", when);
    msg.AddInt32("buttons", 0);
    msg.AddInt32("modifiers", modifiersForWKEventModifiers(wkModifiers));

    BPoint where(m_position.x, m_position.y);
    msg.AddPoint("be:view_where", where);

    BPoint screenWhere = where;
    if (view->Window())
        screenWhere += view->Window()->Frame().LeftTop();
    msg.AddPoint("screen_where", screenWhere);

    m_clickPosition = m_position;
    m_clickTime = m_time;

    view->MessageReceived(&msg);
}

void EventSenderProxy::mouseMoveTo(double x, double y)
{
    m_position.x = x;
    m_position.y = y;

    auto* view = m_testController->mainWebView()->platformView().get();
    if (!view) return;

    BMessage msg(B_MOUSE_MOVED);
    int64 when = (int64)(m_time * 1000000.0);
    msg.AddInt64("when", when);
    msg.AddInt32("buttons", m_leftMouseButtonDown ? B_PRIMARY_MOUSE_BUTTON : 0);
    msg.AddInt32("modifiers", 0);

    BPoint where(x, y);
    msg.AddPoint("be:view_where", where);

    BPoint screenWhere = where;
    if (view->Window())
        screenWhere += view->Window()->Frame().LeftTop();
    msg.AddPoint("screen_where", screenWhere);

    msg.AddInt32("transit", B_INSIDE_VIEW);

    view->MessageReceived(&msg);
}

void EventSenderProxy::mouseScrollBy(int horizontal, int vertical)
{
    auto* view = m_testController->mainWebView()->platformView().get();
    if (!view) return;

    BMessage msg(B_MOUSE_WHEEL_CHANGED);
    int64 when = (int64)(m_time * 1000000.0);
    msg.AddInt64("when", when);
    msg.AddInt32("modifiers", 0);

    msg.AddFloat("be:wheel_delta_x", (float)horizontal);
    msg.AddFloat("be:wheel_delta_y", (float)vertical);

    view->MessageReceived(&msg);
}

void EventSenderProxy::continuousMouseScrollBy(int horizontal, int vertical, bool paged)
{
    mouseScrollBy(horizontal, vertical);
}

void EventSenderProxy::mouseScrollByWithWheelAndMomentumPhases(int x, int y, int /*phase*/, int /*momentum*/)
{
    mouseScrollBy(x, y);
}

void EventSenderProxy::leapForward(int milliseconds)
{
    m_time += milliseconds / 1000.0;
}

void EventSenderProxy::keyDown(WKStringRef keyRef, WKEventModifiers wkModifiers, unsigned location)
{
    auto* view = m_testController->mainWebView()->platformView().get();
    if (!view) return;

    size_t bufferSize = WKStringGetMaximumUTF8CStringSize(keyRef);
    Vector<char> keyBuffer(bufferSize);
    WKStringGetUTF8CString(keyRef, keyBuffer.data(), bufferSize);
    String keyName = String::fromUTF8(keyBuffer.data());

    BMessage msg(B_KEY_DOWN);
    int64 when = (int64)(m_time * 1000000.0);
    msg.AddInt64("when", when);
    msg.AddInt32("modifiers", modifiersForWKEventModifiers(wkModifiers));

    msg.AddString("bytes", keyName.utf8().data());

    view->MessageReceived(&msg);
}

}
