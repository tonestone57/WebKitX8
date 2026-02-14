/*
 * Copyright (C) 2008 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
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
#include "DNS.h"
#include "DNSResolveQueueHaiku.h"

#include "NotImplemented.h"
#include <netdb.h>
#include <thread>
#include <wtf/RunLoop.h>

namespace WebCore {

void DNSResolveQueueHaiku::updateIsUsingProxy()
{
}

void DNSResolveQueueHaiku::platformResolve(const String& /* hostname */)
{
}

void DNSResolveQueueHaiku::resolve(const String& hostname, uint64_t identifier, DNSCompletionHandler&& completionHandler)
{
    // Simple thread-based resolution
    std::thread([hostname = hostname.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        struct addrinfo* result = nullptr;
        int res = getaddrinfo(hostname.utf8().data(), nullptr, &hints, &result);

        if (res != 0) {
            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler(DNSCompletionHandler::Result::UnknownError, { });
            });
            return;
        }

        std::optional<IPAddress> address;
        for (struct addrinfo* p = result; p != nullptr; p = p->ai_next) {
            if (p->ai_family == AF_INET) {
                struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
                address = IPAddress(ipv4->sin_addr);
                break;
            } else if (p->ai_family == AF_INET6) {
                struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)p->ai_addr;
                address = IPAddress(ipv6->sin6_addr);
                break;
            }
        }

        freeaddrinfo(result);

        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), address]() mutable {
            if (address)
                completionHandler(DNSCompletionHandler::Result::Succeeded, { *address });
            else
                completionHandler(DNSCompletionHandler::Result::UnknownError, { });
        });
    }).detach();
}

void DNSResolveQueueHaiku::stopResolve(uint64_t /* identifier */)
{
    // We cannot easily cancel getaddrinfo running in a detached thread.
    // Ideally we would track requests and ignore the callback if cancelled.
}

}
