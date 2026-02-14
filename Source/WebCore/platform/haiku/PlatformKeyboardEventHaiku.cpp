/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2008 Andrea Anzani <andrea.anzani@gmail.com>
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
#include "PlatformKeyboardEvent.h"

#include "WindowsKeyboardCodes.h"
#include <InterfaceDefs.h>
#include <Message.h>
#include <String.h>
#include <wtf/HexNumber.h>
#include <wtf/text/ASCIIFastPath.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringConcatenate.h>

namespace WebCore {

String PlatformKeyboardEvent::keyIdentifierForHaikuKeyCode(char singleByte, int keyCode)
{
    switch (singleByte) {
    case B_FUNCTION_KEY:

        switch (keyCode) {
        case B_F1_KEY:
            return "F1"_s;
        case B_F2_KEY:
            return "F2"_s;
        case B_F3_KEY:
            return "F3"_s;
        case B_F4_KEY:
            return "F4"_s;
        case B_F5_KEY:
            return "F5"_s;
        case B_F6_KEY:
            return "F6"_s;
        case B_F7_KEY:
            return "F7"_s;
        case B_F8_KEY:
            return "F8"_s;
        case B_F9_KEY:
            return "F9"_s;
        case B_F10_KEY:
            return "F10"_s;
        case B_F11_KEY:
            return "F11"_s;
        case B_F12_KEY:
            return "F12"_s;
        case B_PRINT_KEY:
            return "Print"_s;
        case B_PAUSE_KEY:
            return "Pause"_s;
        case B_SCROLL_KEY:
            return "ScrollLock"_s;
        }
        break;

    case B_BACKSPACE:
        return "U+0008"_s;
    case B_LEFT_ARROW:
        return "Left"_s;
    case B_RIGHT_ARROW:
        return "Right"_s;
    case B_UP_ARROW:
        return "Up"_s;
    case B_DOWN_ARROW:
        return "Down"_s;
    case B_INSERT:
        return "Insert"_s;
    case B_ENTER:
        return "Enter"_s;
    case B_DELETE:
        return "U+007F"_s;
    case B_HOME:
        return "Home"_s;
    case B_END:
        return "End"_s;
    case B_PAGE_UP:
        return "PageUp"_s;
    case B_PAGE_DOWN:
        return "PageDown"_s;
    case B_TAB:
        return "U+0009"_s;
    }

    return makeString("U+"_s, hex(toASCIIUpper(singleByte)));
}

int PlatformKeyboardEvent::windowsKeyCodeForKeyEvent(char singleByte, int keyCode)
{
    switch (singleByte) {
    case B_FUNCTION_KEY:
        switch (keyCode) {
        case B_F1_KEY:
            return VK_F1;
        case B_F2_KEY:
            return VK_F2;
        case B_F3_KEY:
            return VK_F3;
        case B_F4_KEY:
            return VK_F4;
        case B_F5_KEY:
            return VK_F5;
        case B_F6_KEY:
            return VK_F6;
        case B_F7_KEY:
            return VK_F7;
        case B_F8_KEY:
            return VK_F8;
        case B_F9_KEY:
            return VK_F9;
        case B_F10_KEY:
            return VK_F10;
        case B_F11_KEY:
            return VK_F11;
        case B_F12_KEY:
            return VK_F12;
        case B_PRINT_KEY:
            return VK_PRINT;
        case B_PAUSE_KEY:
            return VK_PAUSE;
        case B_SCROLL_KEY:
            return VK_SCROLL;
        }
        break;

    case B_BACKSPACE:
        return VK_BACK; // (08) BACKSPACE key
    case B_TAB:
        return VK_TAB; // (09) TAB key
    case B_RETURN:
        return VK_RETURN; //(0D) Return key
    case B_ESCAPE:
        return VK_ESCAPE; // (1B) ESC key
    case B_SPACE:
        return VK_SPACE; // (20) SPACEBAR
    case B_PAGE_UP:
        return VK_PRIOR; // (21) PAGE UP key
    case B_PAGE_DOWN:
        return VK_NEXT; // (22) PAGE DOWN key
    case B_END:
        return VK_END; // (23) END key
    case B_HOME:
        return VK_HOME; // (24) HOME key
    case B_LEFT_ARROW:
        return VK_LEFT; // (25) LEFT ARROW key
    case B_UP_ARROW:
        return VK_UP; // (26) UP ARROW key
    case B_RIGHT_ARROW:
        return VK_RIGHT; // (27) RIGHT ARROW key
    case B_DOWN_ARROW:
        return VK_DOWN; // (28) DOWN ARROW key
    case B_INSERT:
        return VK_INSERT; // (2D) INS key
    case B_DELETE:
        return VK_DELETE; // (2E) DEL key

    case '0':
    case ')':
        return VK_0;
    case '1':
    case '!':
        return VK_1;
    case '2':
    case '@':
        return VK_2;
    case '3':
    case '#':
        return VK_3;
    case '4':
    case '$':
        return VK_4;
    case '5':
    case '%':
        return VK_5;
    case '6':
    case '^':
        return VK_6;
    case '7':
    case '&':
        return VK_7;
    case '8':
    case '*':
        return VK_8;
    case '9':
    case '(':
        return VK_9;
    case 'a':
    case 'A':
        return VK_A;
    case 'b':
    case 'B':
        return VK_B;
    case 'c':
    case 'C':
        return VK_C;
    case 'd':
    case 'D':
        return VK_D;
    case 'e':
    case 'E':
        return VK_E;
    case 'f':
    case 'F':
        return VK_F;
    case 'g':
    case 'G':
        return VK_G;
    case 'h':
    case 'H':
        return VK_H;
    case 'i':
    case 'I':
        return VK_I;
    case 'j':
    case 'J':
        return VK_J;
    case 'k':
    case 'K':
        return VK_K;
    case 'l':
    case 'L':
        return VK_L;
    case 'm':
    case 'M':
        return VK_M;
    case 'n':
    case 'N':
        return VK_N;
    case 'o':
    case 'O':
        return VK_O;
    case 'p':
    case 'P':
        return VK_P;
    case 'q':
    case 'Q':
        return VK_Q;
    case 'r':
    case 'R':
        return VK_R;
    case 's':
    case 'S':
        return VK_S;
    case 't':
    case 'T':
        return VK_T;
    case 'u':
    case 'U':
        return VK_U;
    case 'v':
    case 'V':
        return VK_V;
    case 'w':
    case 'W':
        return VK_W;
    case 'x':
    case 'X':
        return VK_X;
    case 'y':
    case 'Y':
        return VK_Y;
    case 'z':
    case 'Z':
        return VK_Z;
    case ';':
    case ':':
        return VK_OEM_1;
    case '+':
    case '=':
        return VK_OEM_PLUS;
    case ',':
    case '<':
        return VK_OEM_COMMA;
    case '-':
    case '_':
        return VK_OEM_MINUS;
    case '.':
    case '>':
        return VK_OEM_PERIOD;
    case '/':
    case '?':
        return VK_OEM_2;
    case '`':
    case '~':
        return VK_OEM_3;
    case '[':
    case '{':
        return VK_OEM_4;
    case '\\':
    case '|':
        return VK_OEM_5;
    case ']':
    case '}':
        return VK_OEM_6;
    case '\'':
    case '"':
        return VK_OEM_7;
    }
    return singleByte;
}

String PlatformKeyboardEvent::KeyValueForKeyEvent(BString bytes, int keyCode)
{
    switch (bytes.ByteAt(0)) {
        case B_FUNCTION_KEY:
            switch (keyCode) {
                case B_F1_KEY: return "F1"_s;
                case B_F2_KEY: return "F2"_s;
                case B_F3_KEY: return "F3"_s;
                case B_F4_KEY: return "F4"_s;
                case B_F5_KEY: return "F5"_s;
                case B_F6_KEY: return "F6"_s;
                case B_F7_KEY: return "F7"_s;
                case B_F8_KEY: return "F8"_s;
                case B_F9_KEY: return "F9"_s;
                case B_F10_KEY: return "F10"_s;
                case B_F11_KEY: return "F11"_s;
                case B_F12_KEY: return "F12"_s;
                case B_PRINT_KEY: return "Print"_s;
                case B_PAUSE_KEY: return "Pause"_s;
                case B_SCROLL_KEY: return "ScrollLock"_s;
            }
            break;

        case B_BACKSPACE: return "Backspace"_s;
        case B_LEFT_ARROW: return "ArrowLeft"_s;
        case B_RIGHT_ARROW: return "ArrowRight"_s;
        case B_UP_ARROW: return "ArrowUp"_s;
        case B_DOWN_ARROW: return "ArrowDown"_s;
        case B_INSERT: return "Insert"_s;
        case B_ENTER: return "Enter"_s;
        case B_DELETE: return "Delete"_s;
        case B_HOME: return "Home"_s;
        case B_END: return "End"_s;
        case B_PAGE_UP: return "PageUp"_s;
        case B_PAGE_DOWN: return "PageDown"_s;
        case B_TAB: return "Tab"_s;
        case B_SPACE: return " "_s;
        case B_ESCAPE: return "Escape"_s;

        default:
            return String::fromUTF8(bytes);
    }
    return "Unidentified"_s;
}

String PlatformKeyboardEvent::KeyCodeForKeyEvent(int keyCode)
{
    switch (keyCode) {
        case 0x0001: return "Escape"_s;
        case 0x0002: return "F1"_s;
        case 0x0003: return "F2"_s;
        case 0x0004: return "F3"_s;
        case 0x0005: return "F4"_s;
        case 0x0006: return "F5"_s;
        case 0x0007: return "F6"_s;
        case 0x0008: return "F7"_s;
        case 0x0009: return "F8"_s;
        case 0x000A: return "F9"_s;
        case 0x000B: return "F10"_s;
        case 0x000C: return "F11"_s;
        case 0x000D: return "F12"_s;

        case 0x000E: return "PrintScreen"_s;
        case 0x000F: return "ScrollLock"_s;
        case 0x0010: return "Pause"_s;

        case 0x0011: return "Backquote"_s;
        case 0x0012: return "Digit1"_s;
        case 0x0013: return "Digit2"_s;
        case 0x0014: return "Digit3"_s;
        case 0x0015: return "Digit4"_s;
        case 0x0016: return "Digit5"_s;
        case 0x0017: return "Digit6"_s;
        case 0x0018: return "Digit7"_s;
        case 0x0019: return "Digit8"_s;
        case 0x001A: return "Digit9"_s;
        case 0x001B: return "Digit0"_s;
        case 0x001C: return "Minus"_s;
        case 0x001D: return "Equal"_s;
        case 0x001E: return "Backspace"_s;

        case 0x001F: return "Insert"_s;
        case 0x0020: return "Home"_s;
        case 0x0021: return "PageUp"_s;
            
        case 0x0022: return "NumLock"_s;
        case 0x0023: return "NumpadDivide"_s;
        case 0x0024: return "NumpadMultiply"_s;
        case 0x0025: return "NumpadSubtract"_s;

        case 0x0026: return "Tab"_s;
        case 0x0027: return "KeyQ"_s;
        case 0x0028: return "KeyW"_s;
        case 0x0029: return "KeyE"_s;
        case 0x002A: return "KeyR"_s;
        case 0x002B: return "KeyT"_s;
        case 0x002C: return "KeyY"_s;
        case 0x002D: return "KeyU"_s;
        case 0x002E: return "KeyI"_s;
        case 0x002F: return "KeyO"_s;
        case 0x0030: return "KeyP"_s;
        case 0x0031: return "BracketLeft"_s;
        case 0x0032: return "BracketRight"_s;
        case 0x0033: return "Backslash"_s;

        case 0x0034: return "Delete"_s;
        case 0x0035: return "End"_s;
        case 0x0036: return "PageDown"_s;

        case 0x0037: return "Numpad7"_s;
        case 0x0038: return "Numpad8"_s;
        case 0x0039: return "Numpad9"_s;
        case 0x003A: return "NumpadAdd"_s;

        case 0x003B: return "CapsLock"_s;
        case 0x003C: return "KeyA"_s;
        case 0x003D: return "KeyS"_s;
        case 0x003E: return "KeyD"_s;
        case 0x003F: return "KeyF"_s;
        case 0x0040: return "KeyG"_s;
        case 0x0041: return "KeyH"_s;
        case 0x0042: return "KeyJ"_s;
        case 0x0043: return "KeyK"_s;
        case 0x0044: return "KeyL"_s;
        case 0x0045: return "Semicolon"_s;
        case 0x0046: return "Quote"_s;
        case 0x0047: return "Return"_s;

        case 0x0048: return "Numpad4"_s;
        case 0x0049: return "Numpad5"_s;
        case 0x004A: return "Numpad6"_s;

        case 0x004B: return "ShiftLeft"_s;
        case 0x004C: return "KeyZ"_s;
        case 0x004D: return "KeyX"_s;
        case 0x004E: return "KeyC"_s;
        case 0x004F: return "KeyV"_s;
        case 0x0050: return "KeyB"_s;
        case 0x0051: return "KeyN"_s;
        case 0x0052: return "KeyM"_s;
        case 0x0053: return "Comma"_s;
        case 0x0054: return "Period"_s;
        case 0x0055: return "Slash"_s;
        case 0x0056: return "ShiftRight"_s;

        case 0x0057: return "ArrowUp"_s;
        case 0x0058: return "Digit1"_s;
        case 0x0059: return "Digit2"_s;
        case 0x005A: return "Digit3"_s;
        case 0x005B: return "NumpadEnter"_s;
        case 0x005C: return "ControlLeft"_s;
        case 0x005D: return "AltLeft"_s;
        case 0x005E: return "Space"_s;
        case 0x005F: return "AltRight"_s;
        case 0x0060: return "ControlRight"_s;
        case 0x0061: return "ArrowLeft"_s;
        case 0x0062: return "ArrowDown"_s;
        case 0x0063: return "ArrowRight"_s;
        case 0x0064: return "Numpad0"_s;
        case 0x0065: return "NumpadDecimal"_s;
        case 0x0066: return "MetaLeft"_s;
        case 0x0067: return "MetaRight"_s;
        case 0x0068: return "ContextMenu"_s;
        case 0x0069: return "IntlBackslash"_s;
        case 0x006a: return "NumPadEqual"_s;
    }
    return "Unidentified"_s;
}

String PlatformKeyboardEvent::KeyCodeForKeyEvent(BString, int keyCode)
{
    return KeyCodeForKeyEvent(keyCode);
}

PlatformKeyboardEvent::PlatformKeyboardEvent(const BMessage* message)
    : PlatformEvent(PlatformEvent::Type::KeyDown)
    , m_autoRepeat(false)
    , m_isKeypad(false)
{
    BString bytes = message->FindString("bytes");
    int32 nativeVirtualKeyCode = message->FindInt32("key");

    m_text = String::fromUTF8(std::span<const char>(bytes.String(), bytes.Length()));
    m_unmodifiedText = m_text;
    m_keyIdentifier = keyIdentifierForHaikuKeyCode(bytes.ByteAt(0), nativeVirtualKeyCode);
    m_windowsVirtualKeyCode = windowsKeyCodeForKeyEvent(bytes.ByteAt(0), nativeVirtualKeyCode);
    m_key = KeyValueForKeyEvent(bytes, nativeVirtualKeyCode);
    m_code = KeyCodeForKeyEvent(nativeVirtualKeyCode);

    if (message->what == B_KEY_UP)
        m_type = PlatformEvent::Type::KeyUp;
    else if (message->what == B_KEY_DOWN)
        m_type = PlatformEvent::Type::KeyDown;

    int32 modifiers = message->FindInt32("modifiers");
    if (modifiers & B_SHIFT_KEY)
        m_modifiers.add(PlatformEvent::Modifier::ShiftKey);
    if (modifiers & B_COMMAND_KEY)
        m_modifiers.add(PlatformEvent::Modifier::ControlKey);
    if (modifiers & B_CONTROL_KEY)
        m_modifiers.add(PlatformEvent::Modifier::AltKey);
    if (modifiers & B_OPTION_KEY)
        m_modifiers.add(PlatformEvent::Modifier::MetaKey);

    m_timestamp = MonotonicTime::now(); // Approximate, should use 'when' from message if available
    int64 when;
    if (message->FindInt64("when", &when) == B_OK) {
        // Haiku 'when' is system_time() (microseconds since boot)
        // MonotonicTime needs to be compatible.
        // Assuming MonotonicTime::fromRawSeconds uses compatible base or we just use now() for simplicity.
        // using now() is safer for sync.
    }
}

void PlatformKeyboardEvent::disambiguateKeyDownEvent(Type type, bool backwardCompatibilityMode)
{
    ASSERT(m_type == PlatformEvent::Type::KeyDown);
    m_type = type;

    if (backwardCompatibilityMode)
        return;

    if (type == PlatformEvent::Type::RawKeyDown) {
        m_text = String();
        m_unmodifiedText = String();
    } else {
        m_keyIdentifier = String();
        m_windowsVirtualKeyCode = 0;
    }
}

OptionSet<PlatformEvent::Modifier> PlatformKeyboardEvent::currentStateOfModifierKeys()
{
    int32 nativeModifiers = ::modifiers();
    OptionSet<Modifier> modifiers;

    if (nativeModifiers & B_SHIFT_KEY)
        modifiers.add(PlatformEvent::Modifier::ShiftKey);
    if (nativeModifiers & B_COMMAND_KEY)
        modifiers.add(PlatformEvent::Modifier::ControlKey);
    if (nativeModifiers & B_CONTROL_KEY)
        modifiers.add(PlatformEvent::Modifier::AltKey);
    if (nativeModifiers & B_OPTION_KEY)
        modifiers.add(PlatformEvent::Modifier::MetaKey);

    return modifiers;
}

} // namespace WebCore
