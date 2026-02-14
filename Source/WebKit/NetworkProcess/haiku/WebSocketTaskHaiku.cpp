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
#include "WebSocketTaskHaiku.h"

#include "NetworkSocketChannel.h"
#include <wtf/RunLoop.h>

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebSocketTask);

Ref<WebSocketTask> WebSocketTask::create(NetworkSocketChannel& channel, const ResourceRequest& request, const String& protocol)
{
    return adoptRef(*new WebSocketTask(channel, request, protocol));
}

WebSocketTask::WebSocketTask(NetworkSocketChannel& channel, const ResourceRequest& request, const String& protocol)
    : m_channel(channel)
    , m_request(request)
    , m_protocol(protocol)
{
}

WebSocketTask::~WebSocketTask()
{
}

void WebSocketTask::sendString(std::span<const uint8_t>, CompletionHandler<void()>&& callback)
{
    callback();
}

void WebSocketTask::sendData(std::span<const uint8_t>, CompletionHandler<void()>&& callback)
{
    callback();
}

void WebSocketTask::close(int32_t code, const String& reason)
{
    m_channel.didClose(static_cast<unsigned short>(code), reason);
}

void WebSocketTask::cancel()
{
}

void WebSocketTask::resume()
{
    // Signal connection failure for now as native WebSocket support is not fully implemented.
    RunLoop::main().dispatch([this, weakThis = ThreadSafeWeakPtr { *this }] {
        auto strongThis = weakThis.get();
        if (!strongThis)
            return;
        m_channel.didReceiveMessageError("Native WebSockets are not yet implemented on Haiku."_s);
    });
}

} // namespace WebKit
