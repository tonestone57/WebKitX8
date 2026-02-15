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
#include "WebPage.h"

#include "WebPageCreationParameters.h"
#include <WebCore/BackForwardController.h>
#include <WebCore/EventHandler.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/Settings.h>
#include <WebCore/UserAgent.h>
#include <WebCore/WindowsKeyboardCodes.h>

#include "WebKeyboardEvent.h"

#include "PrintInfo.h"
#include <WebCore/PrintContext.h>
#include <WebCore/ShareableBitmap.h>

using namespace WebCore;

namespace WebKit {

void WebPage::platformInitialize(WebKit::WebPageCreationParameters const& parameters)
{
    m_statusBarIsVisible = parameters.statusBarIsVisible;
    m_menuBarIsVisible = parameters.menuBarIsVisible;
    m_toolbarsAreVisible = parameters.toolbarsAreVisible;
}

void WebPage::platformReinitializeAccessibilityToken()
{
}

void WebPage::platformDetach()
{
}

#if HAVE(ACCESSIBILITY)
void WebPage::updateAccessibilityTree()
{
    if (!m_accessibilityObject)
        return;

    webPageAccessibilityObjectRefresh(m_accessibilityObject.get());
}
#endif

bool WebPage::platformCanHandleRequest(const ResourceRequest& request)
{
    if (request.url().protocolIsInHTTPFamily())
        return true;
    if (request.url().protocolIs("file"_s))
        return true;
    if (request.url().protocolIs("data"_s))
        return true;
    return false;
}

const char* WebPage::interpretKeyEvent(const KeyboardEvent* event)
{
    const PlatformKeyboardEvent* platformEvent = event->underlyingPlatformEvent();
    if (!platformEvent)
        return nullptr;

    switch (platformEvent->windowsVirtualKeyCode()) {
    case VK_BACK:
        return "DeleteBackward";
    case VK_BACKTAB:
        return "InsertBacktab";
    case VK_TAB:
        return "InsertTab";
    case VK_RETURN:
        return "InsertNewline";
    case VK_DELETE:
        return "DeleteForward";
    case VK_HOME:
        return "MoveToBeginningOfLine";
    case VK_END:
        return "MoveToEndOfLine";
    case VK_PRIOR:
        return "MoveUpByPage";
    case VK_NEXT:
        return "MoveDownByPage";
    case VK_LEFT:
        return "MoveLeft";
    case VK_RIGHT:
        return "MoveRight";
    case VK_UP:
        return "MoveUp";
    case VK_DOWN:
        return "MoveDown";
    }

    return nullptr;
}

String WebPage::platformUserAgent(const URL& url) const
{
    if (url.isNull() || !m_page->settings().needsSiteSpecificQuirks())
        return emptyString();

    return WebCore::standardUserAgentForURL(url);
}

bool WebPage::hoverSupportedByPrimaryPointingDevice() const
{
    return true;
}

bool WebPage::hoverSupportedByAnyAvailablePointingDevice() const
{
    return true;
}

std::optional<PointerCharacteristics> WebPage::pointerCharacteristicsOfPrimaryPointingDevice() const
{
    return PointerCharacteristics::Fine;
}

OptionSet<PointerCharacteristics> WebPage::pointerCharacteristicsOfAllAvailablePointingDevices() const
{
    return PointerCharacteristics::Fine;
}

bool WebPage::handleEditingKeyboardEvent(WebCore::KeyboardEvent& event)
{
    const PlatformKeyboardEvent* platformEvent = event.underlyingPlatformEvent();
    if (!platformEvent)
        return false;

    if (platformEvent->type() == PlatformEvent::Type::RawKeyDown || platformEvent->type() == PlatformEvent::Type::Char) {
        // Haiku uses Alt (Command) for shortcuts.
        bool isCommandKey = platformEvent->modifiers().contains(PlatformEvent::Modifier::ControlKey); // Mapped to Command in PlatformKeyboardEventHaiku

        if (isCommandKey) {
            String commandName;
            switch (platformEvent->windowsVirtualKeyCode()) {
            case VK_C:
                commandName = "Copy"_s;
                break;
            case VK_V:
                commandName = "Paste"_s;
                break;
            case VK_X:
                commandName = "Cut"_s;
                break;
            case VK_A:
                commandName = "SelectAll"_s;
                break;
            case VK_Z:
                if (platformEvent->modifiers().contains(PlatformEvent::Modifier::ShiftKey))
                    commandName = "Redo"_s;
                else
                    commandName = "Undo"_s;
                break;
            }

            if (!commandName.isEmpty()) {
                m_page->executeEditingCommand(commandName);
                return true;
            }
        }
    }
    return false;
}

void WebPage::getPlatformEditorState(LocalFrame& frame, EditorState& result) const
{
}

void WebPage::drawRectToImage(WebCore::FrameIdentifier frameID, const PrintInfo&, const WebCore::IntRect& rect, const WebCore::IntSize& imageSize, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>&&)>&& completionHandler)
{
    PrintContextAccessScope scope { *this };
    RefPtr frame = WebProcess::singleton().webFrame(frameID);
    RefPtr coreFrame = frame ? frame->coreLocalFrame() : nullptr;

    RefPtr<WebCore::ShareableBitmap> image;

    if (coreFrame) {
        ShareableBitmap::Configuration configuration;
        configuration.size = imageSize;
        configuration.colorSpace = WebCore::DestinationColorSpace::SRGB();
        image = WebCore::ShareableBitmap::create(configuration);
        if (!image) {
            completionHandler(std::nullopt);
            return;
        }

        auto context = image->createGraphicsContext();
        if (!context) {
            completionHandler(std::nullopt);
            return;
        }

        float printingScale = static_cast<float>(imageSize.width()) / rect.width();
        context->scale(printingScale);

        if (m_printContext)
            Ref { *m_printContext }->spoolRect(*context, rect);
    }

    std::optional<WebCore::ShareableBitmap::Handle> handle;
    if (image)
        handle = image->createHandle(WebCore::SharedMemory::Protection::ReadOnly);

    completionHandler(WTF::move(handle));
}

} // namespace WebKit
