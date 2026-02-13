/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2008 Christian Dywan <christian@imendio.com>
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
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
#include "LocalizedStrings.h"

#include <wtf/text/WTFString.h>

namespace WebCore {

String submitButtonDefaultLabel()
{
    return "Submit"_s;
}

String inputElementAltText()
{
    return "Submit"_s;
}

String resetButtonDefaultLabel()
{
    return "Reset"_s;
}

String fileButtonChooseFileLabel()
{
    return "Choose File"_s;
}

String fileButtonChooseFilesLabel()
{
    return "Choose Files"_s;
}

String fileButtonNoFileSelectedLabel()
{
    return "No file selected"_s;
}

String searchMenuNoRecentSearchesText()
{
    return "No recent searches"_s;
}

String searchMenuRecentSearchesText()
{
    return "Recent searches"_s;
}

String searchMenuClearRecentSearchesText()
{
    return "Clear recent searches"_s;
}

String AXWebAreaText()
{
    return "Web Area"_s;
}

String AXLinkText()
{
    return "Link"_s;
}

String AXListMarkerText()
{
    return "List Marker"_s;
}

String AXImageMapText()
{
    return "Image Map"_s;
}

String AXHeadingText()
{
    return "Heading"_s;
}

String AXButtonActionVerb()
{
    return "press"_s;
}

String AXRadioButtonActionVerb()
{
    return "select"_s;
}

String AXTextFieldActionVerb()
{
    return "activate"_s;
}

String AXCheckedCheckBoxActionVerb()
{
    return "uncheck"_s;
}

String AXUncheckedCheckBoxActionVerb()
{
    return "check"_s;
}

String AXLinkActionVerb()
{
    return "jump"_s;
}

String multipleFileUploadText(unsigned numberOfFiles)
{
    return makeString(numberOfFiles, " files"_s);
}

String unknownFileSizeText()
{
    return "Unknown"_s;
}

String imageTitle(const String& filename, const IntSize& size)
{
    return makeString(filename, " "_s, size.width(), "x"_s, size.height());
}

String mediaElementLoadingStateText()
{
    return "Loading..."_s;
}

String mediaElementLiveBroadcastStateText()
{
    return "Live Broadcast"_s;
}

String localizedMediaTimeDescription(float time)
{
    return String::number(time);
}

String validationMessageValueMissingText()
{
    return "Value missing"_s;
}

String validationMessageTypeMismatchText()
{
    return "Type mismatch"_s;
}

String validationMessagePatternMismatchText()
{
    return "Pattern mismatch"_s;
}

String validationMessageTooLongText(int, int)
{
    return "Too long"_s;
}

String validationMessageRangeUnderflowText(const String&)
{
    return "Range underflow"_s;
}

String validationMessageRangeOverflowText(const String&)
{
    return "Range overflow"_s;
}

String validationMessageStepMismatchText(const String&, const String&)
{
    return "Step mismatch"_s;
}

String missingPluginText()
{
    return "Missing Plug-in"_s;
}

String crashedPluginText()
{
    return "Plug-in Crashed"_s;
}

String blockedPluginByContentSecurityPolicyText()
{
    return "Plug-in blocked"_s;
}

String insecurePluginVersionText()
{
    return "Plug-in insecure"_s;
}

String inactivePluginText()
{
    return "Plug-in inactive"_s;
}

String clickToActivatePluginText()
{
    return "Click to activate"_s;
}

String defaultDetailsSummaryText()
{
    return "Details"_s;
}

#if ENABLE(VIDEO)
String localizedMediaTimeDescription(double time)
{
    return String::number(time);
}
#endif

String validationMessageBadInputForNumberText() { return "Bad input"_s; }

#if ENABLE(FILE_REPLACEMENT)
String fileButtonReplaceButtonLabel() { return "Replace"_s; }
#endif

String unsupportedPluginText() { return "Unsupported Plug-in"_s; }

String contextMenuItemTagBold()
{
    return "Bold"_s;
}

String contextMenuItemTagItalic()
{
    return "Italic"_s;
}

String contextMenuItemTagUnderline()
{
    return "Underline"_s;
}

String contextMenuItemTagOutline()
{
    return "Outline"_s;
}

String contextMenuItemTagLeftToRight()
{
    return "Left to Right"_s;
}

String contextMenuItemTagRightToLeft()
{
    return "Right to Left"_s;
}

String contextMenuItemTagDefaultDirection()
{
    return "Default Direction"_s;
}

} // namespace WebCore
