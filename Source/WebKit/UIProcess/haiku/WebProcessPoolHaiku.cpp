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
#include "WebProcessPool.h"

#include "WebProcessCreationParameters.h"
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>

#include <OS.h>

namespace WebKit {

void WebProcessPool::platformInitialize(NeedsGlobalStaticInitialization)
{
    // Check memory status periodically
    static bool memoryPressureHandlerInitialized = false;
    if (!memoryPressureHandlerInitialized) {
        memoryPressureHandlerInitialized = true;

        RunLoop::main().dispatchRepeating([] {
            system_info info;
            if (get_system_info(&info) == B_OK) {
                // If free memory is less than 5% or 64MB (assuming pages are 4KB), trigger low memory warning.
                // Haiku pages are usually 4096 bytes.
                uint64_t freeMemory = (uint64_t)info.free_memory * B_PAGE_SIZE;
                uint64_t totalMemory = (uint64_t)info.max_pages * B_PAGE_SIZE;

                if (freeMemory < 64 * 1024 * 1024 || (totalMemory > 0 && (double)freeMemory / totalMemory < 0.05)) {
                    WebProcessPool::sendMemoryPressureEvent(true);
                }
            }
        }, 10_s);
    }
}

void WebProcessPool::platformInitializeNetworkProcess(NetworkProcessCreationParameters&)
{
}

void WebProcessPool::platformInitializeWebProcess(const WebKit::WebProcessProxy&, WebProcessCreationParameters&)
{
}

void WebProcessPool::platformInvalidateContext()
{
}

void WebProcessPool::platformResolvePathsForSandboxExtensions()
{
}

} // namespace WebKit
