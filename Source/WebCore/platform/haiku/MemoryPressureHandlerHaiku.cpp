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
#include "MemoryPressureHandler.h"

#include <OS.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>

namespace WebCore {

void MemoryPressureHandler::platformReleaseMemory(Critical)
{
}

std::optional<MemoryPressureHandler::ReliefLogger::MemoryUsage> MemoryPressureHandler::ReliefLogger::platformMemoryUsage()
{
    return std::nullopt;
}

void MemoryPressureHandler::install()
{
    if (m_installed)
        return;

    m_installed = true;

    RunLoop::main().dispatchRepeating([] {
        system_info info;
        if (get_system_info(&info) == B_OK) {
            // Include cached pages as available memory since Haiku caches aggressively
            uint64_t freeMemory = (uint64_t)info.free_memory + ((uint64_t)info.cached_pages * B_PAGE_SIZE);
            uint64_t totalMemory = (uint64_t)info.max_pages * B_PAGE_SIZE;

            // Trigger if less than 64MB or 10% memory free
            // Haiku VMs often run with 512MB RAM, so 128MB is too high (25%).
            // 64MB is a safer floor for critical pressure.
            if (freeMemory < 64 * 1024 * 1024 || (totalMemory > 0 && (double)freeMemory / totalMemory < 0.10)) {
                MemoryPressureHandler::singleton().triggerMemoryPressureEvent(true);
            }
        }
    }, 10_s);
}

} // namespace WebCore
