/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Holger Hans Peter Freyther
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
#include "ResourceHandle.h"

#include "DocumentLoader.h"
#include "Frame.h"
#include "NotImplemented.h"
#include "Page.h"
#include "BUrlProtocolHandler.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "SharedBuffer.h"

namespace WebCore {

ResourceHandleInternal::~ResourceHandleInternal()
{
}

ResourceHandle::~ResourceHandle()
{
    cancel();
}

bool ResourceHandle::start()
{
    // If NetworkingContext is invalid then we are no longer attached to a Page,
    // this must be an attempted load from an unload event handler, so let's just block it.
    if (d->m_context && !d->m_context->isValid())
        return false;

    if (!d->m_user.isEmpty() || !d->m_password.isEmpty()) {
        // If credentials were specified for this request, add them to the url,
        // so that they will be passed to QNetworkRequest.
        URL urlWithCredentials(firstRequest().url());
        urlWithCredentials.setUser(d->m_user);
        urlWithCredentials.setPassword(d->m_password);
        d->m_firstRequest.setURL(urlWithCredentials);
    }

    d->m_urlrequest = adoptRef(*new BUrlProtocolHandler(this));

    if (!d->m_urlrequest->isValid())
        scheduleFailure(InvalidURLFailure);
    return true;
}

void ResourceHandle::cancel()
{
    if (d->m_urlrequest) {
        d->m_urlrequest->abort();
        d->m_urlrequest = nullptr;
    }
}

void ResourceHandle::platformLoadResourceSynchronously(NetworkingContext* context, const ResourceRequest& request, StoredCredentialsPolicy storedCredentials, WebCore::SecurityOrigin*, ResourceError& error, ResourceResponse& response, Vector<unsigned char>& data)
{
    BUrlRequest* urlRequest = request.toNetworkRequest(context);
    if (!urlRequest) {
        error = ResourceError(String(), 0, request.url(), "Failed to create request"_s);
        return;
    }

    class SyncListener : public BUrlProtocolListener {
    public:
        SyncListener(ResourceResponse& response, Vector<unsigned char>& data)
            : m_response(response)
            , m_data(data)
            , m_sem(create_sem(0, "SyncListener"))
        {
        }

        ~SyncListener()
        {
            delete_sem(m_sem);
        }

        void Wait() {
            while (acquire_sem(m_sem) == B_INTERRUPTED);
        }

        void ConnectionOpened(BUrlRequest*) override { }
        void HostnameResolved(BUrlRequest*, const char*) override { }
        void ResponseStarted(BUrlRequest* caller) override {
            const BHttpResult* result = dynamic_cast<const BHttpResult*>(&caller->Result());
            if (result) {
                m_response.setURL(URL(result->Url()));
                m_response.setMimeType(String::fromUTF8(result->ContentType()));
                m_response.setHTTPStatusCode(result->StatusCode());
                m_response.setHTTPStatusText(String::fromUTF8(result->StatusText()));
                const BHttpHeaders& headers = result->Headers();
                for (int i = 0; i < headers.CountHeaders(); i++) {
                    m_response.setHTTPHeaderField(String::fromUTF8(headers.HeaderAt(i).Name()), String::fromUTF8(headers.HeaderAt(i).Value()));
                }
            }
        }
        void HeadersReceived(BUrlRequest*) override { }
        void DataReceived(BUrlRequest*, const char* data, off_t, ssize_t size) override {
            if (size > 0)
                m_data.append(std::span<const uint8_t>((const uint8_t*)data, size));
        }
        void DownloadProgress(BUrlRequest*, ssize_t, ssize_t) override { }
        void UploadProgress(BUrlRequest*, ssize_t, ssize_t) override { }
        void RequestCompleted(BUrlRequest*, bool success) override {
            m_success = success;
            release_sem(m_sem);
        }
        void DebugMessage(BUrlRequest*, BUrlProtocolDebugMessage, const char*) override { }

        bool m_success { false };
    private:
        ResourceResponse& m_response;
        Vector<unsigned char>& m_data;
        sem_id m_sem;
    };

    SyncListener listener(response, data);
    urlRequest->SetListener(&listener);

    if (urlRequest->Run() < B_OK) {
        error = ResourceError(String(), 0, request.url(), "Failed to run request"_s);
        delete urlRequest;
        return;
    }

    listener.Wait();

    if (!listener.m_success) {
        error = ResourceError(String(), 0, request.url(), "Request failed"_s);
    }

    delete urlRequest;
}


bool ResourceHandle::didReceiveInvalidCertificate(BCertificate& certificate,
    const char* message)
{
    if (client())
        return client()->didReceiveInvalidCertificate(this, certificate, message);
    return false;
}


void ResourceHandle::didReceiveAuthenticationChallenge(const AuthenticationChallenge& challenge)
{
    ResourceHandleInternal* internal = getInternal();
    ASSERT(internal->m_currentWebChallenge.isNull());
    ASSERT(challenge.authenticationClient() == this); // Should be already set.
    internal->m_currentWebChallenge = challenge;

    if (challenge.previousFailureCount()) {
        // The stored credentials weren't accepted, clear it from storage
        // to prevent reuse if the client refuses to provide credentials.
        d->m_user = String();
        d->m_password = String();
    }

    if (client())
        client()->didReceiveAuthenticationChallenge(this, challenge);
}

void ResourceHandle::receivedCredential(const AuthenticationChallenge& challenge, const Credential& credential)
{
    ASSERT(!challenge.isNull());
    ResourceHandleInternal* internal = getInternal();

    internal->m_user = credential.user();
    internal->m_password = credential.password();

    clearAuthentication();
}

void ResourceHandle::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge& challenge)
{
    ASSERT(!challenge.isNull());
    ResourceHandleInternal* internal = getInternal();

    internal->m_user = String();
    internal->m_password = String();

    clearAuthentication();
}

void ResourceHandle::receivedCancellation(const AuthenticationChallenge&)
{
    // TODO
}


void ResourceHandle::receivedRequestToPerformDefaultHandling(const AuthenticationChallenge&)
{
    ASSERT_NOT_REACHED();
}


void ResourceHandle::receivedChallengeRejection(const AuthenticationChallenge&)
{
    ASSERT_NOT_REACHED();
}


void ResourceHandle::platformSetDefersLoading(bool defers)
{
    d->m_defersLoading = defers;

    /*if (d->m_job)
        d->m_job->setLoadMode(QNetworkReplyHandler::LoadMode(defers));*/
}

} // namespace WebCore
