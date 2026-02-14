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
#include "Download.h"
#include "NetworkDataTask.h"

#include <wtf/text/CString.h>

namespace WebKit {

void Download::platformCancelNetworkLoad(CompletionHandler<void(std::span<const uint8_t>)>&& completionHandler)
{
    // Serialize URL as resume data.
    // Format: [8 bytes offset][URL string]

    String url = m_download->firstRequest().url().string();
    CString utf8 = url.utf8();

    // Use m_download->bytesTransferredOverNetwork() for offset
    uint64_t offset = static_cast<uint64_t>(m_download->bytesTransferredOverNetwork());

    Vector<uint8_t> resumeData;
    resumeData.reserveInitialCapacity(sizeof(uint64_t) + utf8.length());

    // Append offset (little endian assuming Haiku x86)
    // Using simple append for now.
    for (size_t i = 0; i < sizeof(uint64_t); ++i)
        resumeData.append(static_cast<uint8_t>((offset >> (i * 8)) & 0xFF));

    resumeData.append(reinterpret_cast<const uint8_t*>(utf8.data()), utf8.length());

    completionHandler(std::span<const uint8_t>(resumeData.data(), resumeData.size()));
}

void Download::platformDestroyDownload()
{
}

void Download::platformDidFinish(CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

void Download::resume(std::span<const uint8_t> resumeData, const String& path, SandboxExtension::Handle&& sandboxExtensionHandle, std::span<const uint8_t> activityAccessToken)
{
}

} // namespace WebKit
