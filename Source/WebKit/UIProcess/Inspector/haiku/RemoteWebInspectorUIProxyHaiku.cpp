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
#include "RemoteWebInspectorUIProxy.h"

#if ENABLE(REMOTE_INSPECTOR)

#include <WebCore/NotImplemented.h>

namespace WebKit {

WebPageProxy* RemoteWebInspectorUIProxy::platformCreateFrontendPageAndWindow()
{
    // FIXME: Implement creation of inspector window
    return nullptr;
}

void RemoteWebInspectorUIProxy::platformResetState()
{
}

void RemoteWebInspectorUIProxy::platformBringToFront()
{
    // FIXME: Bring inspector window to front
}

void RemoteWebInspectorUIProxy::platformSave(Vector<WebCore::InspectorFrontendClient::SaveData>&&, bool /* forceSaveAs */)
{
    // FIXME: Implement save dialog
}

void RemoteWebInspectorUIProxy::platformLoad(const String&, CompletionHandler<void(const String&)>&& completionHandler)
{
    completionHandler(nullString());
}

void RemoteWebInspectorUIProxy::platformPickColorFromScreen(CompletionHandler<void(const std::optional<WebCore::Color>&)>&& completionHandler)
{
    completionHandler({ });
}

void RemoteWebInspectorUIProxy::platformSetSheetRect(const FloatRect&)
{
}

void RemoteWebInspectorUIProxy::platformSetForcedAppearance(InspectorFrontendClient::Appearance)
{
}

void RemoteWebInspectorUIProxy::platformStartWindowDrag()
{
}

void RemoteWebInspectorUIProxy::platformOpenURLExternally(const String&)
{
    // FIXME: BWorkspace::Launch
}

void RemoteWebInspectorUIProxy::platformRevealFileExternally(const String&)
{
    // FIXME: BWorkspace::Launch
}

void RemoteWebInspectorUIProxy::platformShowCertificate(const CertificateInfo&)
{
}

void RemoteWebInspectorUIProxy::platformCloseFrontendPageAndWindow()
{
    // FIXME: Close window
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
