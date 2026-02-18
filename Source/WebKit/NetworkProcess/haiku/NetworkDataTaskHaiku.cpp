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
#include "NetworkDataTaskHaiku.h"
#include "NetworkProcessHaiku.h"

#include "AuthenticationManager.h"
#include "NetworkResourceLoader.h"

#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/ProtectionSpace.h>
#include <WebCore/CookieJar.h>
#include <WebCore/HTTPParsers.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SameSiteInfo.h>
#include <WebCore/SharedBuffer.h>

#include <wtf/text/CString.h>
#include <wtf/MainThread.h>

#include <Url.h>
#include <UrlRequest.h>
#include <HttpRequest.h>
#include <assert.h>
#include "BeDC.h"

#include <File.h>
#include <Path.h>
#include <FindDirectory.h>
#include <wtf/HashSet.h>
#include <wtf/text/WTFString.h>
#include <wtf/text/StringBuilder.h>

static const int gMaxRecursionLimit = 10;

namespace WebKit {

using namespace WebCore;

class NetworkDataOutput : public BDataIO {
public:
    NetworkDataOutput(NetworkDataTaskHaiku* task)
        : m_task(task)
    {
    }

    ssize_t Write(const void* buffer, size_t size) override
    {
        if (m_task)
            m_task->didReceiveData(buffer, size);
        return size;
    }

    ssize_t Read(void* buffer, size_t size) override { return 0; }
    off_t Seek(off_t position, uint32 seekMode) override { return 0; }
    off_t Position() const override { return 0; }
    status_t SetSize(off_t size) override { return B_OK; }

private:
    NetworkDataTaskHaiku* m_task;
};

NetworkDataTaskHaiku::NetworkDataTaskHaiku(NetworkSession& session, NetworkDataTaskClient& client,
    const ResourceRequest& requestWithCredentials, StoredCredentialsPolicy storedCredentialsPolicy,
    ContentSniffingPolicy shouldContentSniff, ContentEncodingSniffingPolicy,
    bool shouldClearReferrerOnHTTPSToHTTPRedirect, bool dataTaskIsForMainFrameNavigation)
    : NetworkDataTask(session, client, requestWithCredentials, storedCredentialsPolicy,
        shouldClearReferrerOnHTTPSToHTTPRedirect, dataTaskIsForMainFrameNavigation)
    , m_postData(NULL)
    , m_output(NULL)
    , m_responseDataSent(false)
    , m_redirected(false)
    , m_position(0)
    , m_redirectionTries(gMaxRecursionLimit)
{
    auto request = requestWithCredentials;
    if (request.url().protocolIsInHTTPFamily()) {
        m_startTime = MonotonicTime::now();
        auto url = request.url();
        if (m_storedCredentialsPolicy == StoredCredentialsPolicy::Use) {
            m_user = url.user();
            m_password = url.password();
            request.removeCredentials();
        }
    }
    createRequest(WTFMove(request));
}

NetworkDataTaskHaiku::~NetworkDataTaskHaiku()
{
    cancel();
    if (m_request)
        m_request->SetListener(NULL);
    delete m_request;
    delete m_output;
}

void NetworkDataTaskHaiku::createRequest(ResourceRequest&& request)
{
    m_currentRequest = WTFMove(request);
    m_request = m_currentRequest.toNetworkRequest(nullptr);

    BString method = BString(m_currentRequest.httpMethod());

    m_postData = NULL;

    if (m_request == NULL)
        return;

    m_output = new NetworkDataOutput(this);
    m_request->SetOutput(m_output);

    m_baseUrl = URL(m_request->Url());

    BHttpRequest* httpRequest = dynamic_cast<BHttpRequest*>(m_request);
    if (httpRequest) {

        if (method == B_HTTP_POST || method == B_HTTP_PUT) {
            FormData* form = m_currentRequest.httpBody();
            if (form) {
                m_postData = new BFormDataIO(form, sessionID());
                httpRequest->AdoptInputData(m_postData, m_postData->Size());
            }
        }

        httpRequest->SetMethod(method.String());
    }

    if (this->SynchronousListener()) {
        m_request->SetListener(this->SynchronousListener());
    } else {
        m_request->SetListener(this);
    }

    if (m_request->Run() < B_OK)
    {
        if (!m_client)
            return;

        ResourceError error("BUrlProtocol"_s, 42, m_baseUrl,
            "The service kit failed to start the request."_s);

        m_networkLoadMetrics.responseEnd = MonotonicTime::now();
        m_networkLoadMetrics.markComplete();
        m_client->didCompleteWithError(error, m_networkLoadMetrics);
    }
}

void NetworkDataTaskHaiku::cancel()
{
    if (m_state == State::Canceling || m_state == State::Completed)
        return;

    m_state = State::Canceling;

    if(m_request)
        m_request->Stop();
}

void NetworkDataTaskHaiku::resume()
{
    if (m_state == State::Completed || m_state == State::Canceling)
        return;

    m_state = State::Running;
}

void NetworkDataTaskHaiku::invalidateAndCancel()
{
}

NetworkDataTask::State NetworkDataTaskHaiku::state() const
{
    return m_state;
}

void NetworkDataTaskHaiku::runOnMainThread(Function<void()>&& task)
{
    if(isMainThread())
        task();
    else
        callOnMainThread(WTFMove(task));
}


void NetworkDataTaskHaiku::ConnectionOpened(BUrlRequest*)
{
    m_responseDataSent = false;
}

void NetworkDataTaskHaiku::HeadersReceived(BUrlRequest* caller)
{
    if (m_currentRequest.isNull())
        return;

    const BHttpResult* httpResult = dynamic_cast<const BHttpResult*>(&caller->Result());

    WTF::String contentType = String::fromUTF8(caller->Result().ContentType().String());
    int contentLength = caller->Result().Length();
    URL url;

    WTF::StringView encoding = extractCharsetFromMediaType(contentType);
    WTF::StringView mimeType = extractMIMETypeFromMediaType(contentType);

    if (httpResult) {
        url = URL(httpResult->Url());

        BString location = httpResult->Headers()["Location"];
        if (location.Length() > 0) {
            m_redirected = true;
            url = URL(url, String::fromUTF8(location.String()));
        } else {
            m_redirected = false;
        }
    } else {
        url = m_baseUrl;
    }

    ResourceResponse response(url, mimeType.toString(), contentLength, encoding.toString());

    if (httpResult) {
        int statusCode = httpResult->StatusCode();

        StringView suggestedFilename = filenameFromHTTPContentDisposition(
            String::fromUTF8(httpResult->Headers()["Content-Disposition"]));

        if (!suggestedFilename.isEmpty())
            response.setSuggestedFilename(suggestedFilename.toString());

        response.setHTTPStatusCode(statusCode);
        response.setHTTPStatusText(AtomString::fromUTF8(httpResult->StatusText()));

        // Add remaining headers.
        const BHttpHeaders& resultHeaders = httpResult->Headers();
        for (int i = 0; i < resultHeaders.CountHeaders(); i++) {
            BHttpHeader& headerPair = resultHeaders.HeaderAt(i);
            response.setHTTPHeaderField(String::fromUTF8(headerPair.Name()), String::fromUTF8(headerPair.Value()));
        }

        if (statusCode == 401) {
            AuthenticationNeeded(dynamic_cast<BHttpRequest*>(m_request), response);
            // AuthenticationNeeded may have aborted the request
            // so we need to make sure we can continue.

            if (m_currentRequest.isNull())
                return;
        }
    }

    if (!m_client)
        return;

    if (m_redirected) {
        m_redirectionTries--;

        if (m_redirectionTries == 0) {
            ResourceError error(url.host().toString(), 400, url,
                String::fromUTF8("Redirection limit reached"));

            m_networkLoadMetrics.responseEnd = MonotonicTime::now();
            m_networkLoadMetrics.markComplete();
            m_client->didCompleteWithError(error,m_networkLoadMetrics);
            return;
        }

        // Notify the client that we are redirecting.
        ResourceRequest request = m_currentRequest;
        ResourceResponse responseCopy = response;
        request.setURL(url);

        runOnMainThread([this, protectedThis = Ref { *this }, responseCopy = WTFMove(responseCopy), request = WTFMove(request)]() mutable {
            if (m_state == State::Canceling || m_state == State::Completed)
                return;

            if (m_client) {
                m_client->willPerformHTTPRedirection(WTFMove(responseCopy),WTFMove(request),
                    [protectedThis](const ResourceRequest& newRequest)
                    {
                        if(newRequest.isNull() || protectedThis->m_state == State::Canceling)
                            return;

                        protectedThis->m_startTime = MonotonicTime::now();//network metrics

                        if( protectedThis->m_state != State::Suspended ){
                            protectedThis->m_state = State::Suspended;
                            protectedThis->resume();
                        }
                    }
                );
            }
        });
    } else {
        ResourceResponse responseCopy = response;
        runOnMainThread([this, protectedThis = Ref { *this }, responseCopy = WTFMove(responseCopy)]() mutable {
            if (m_state == State::Canceling || m_state == State::Completed)
                return;

            if (m_client) {
                m_client->didReceiveResponse(WTFMove(responseCopy), NegotiatedLegacyTLS::No,
                    PrivateRelayed::No, [protectedThis](WebCore::PolicyAction policyAction){
                    if(protectedThis->m_state == State::Canceling || protectedThis->m_state == State::Completed){
                        return;
                    }
                });
            }
        });
    }
}

void NetworkDataTaskHaiku::DataReceived(BUrlRequest* caller, const char* data, off_t position, ssize_t size)
{
    if (m_output)
        return;

    if (m_currentRequest.isNull())
        return;

    if (!m_client)
        return;

    // don't emit the "Document has moved here" type of HTML
    if (m_redirected)
        return;

    if (size > 0) {
        m_responseDataSent = true;
        Vector<uint8_t> buffer;
        buffer.append((const uint8_t*)data, size);

        runOnMainThread([this, protectedThis = Ref { *this }, buffer = WTFMove(buffer)]() mutable {
            if (m_state == State::Canceling || m_state == State::Completed)
                return;

            if (m_client)
                m_client->didReceiveData(SharedBuffer::create(WTFMove(buffer)));
        });
    }

    m_position += size;
}

void NetworkDataTaskHaiku::BytesWritten(BUrlRequest* caller, size_t size)
{
    // Handled by NetworkDataOutput::Write
}

void NetworkDataTaskHaiku::UploadProgress(BUrlRequest* caller, off_t bytesSent, off_t bytesTotal)
{
    if (m_state == State::Canceling || m_state == State::Completed)
        return;

    off_t delta = bytesSent - m_lastBytesSent;
    m_lastBytesSent = bytesSent;

    if (delta > 0 && m_client) {
        runOnMainThread([this, protectedThis = Ref { *this }, delta, bytesSent, bytesTotal]() {
             if (m_state == State::Canceling || m_state == State::Completed)
                 return;

             if (m_client)
                 m_client->didSendData(delta, bytesSent, bytesTotal);
        });
    }
}

void NetworkDataTaskHaiku::RequestCompleted(BUrlRequest* caller, bool success)
{
    if (m_state == State::Canceling || m_state == State::Completed)
        return;

    m_state = State::Completed;

    if (!success) {
        ResourceError error(m_baseUrl.host().toString(), caller->Result().StatusCode(), m_baseUrl,
            String::fromUTF8(caller->Result().StatusText()));

        m_networkLoadMetrics.responseEnd = MonotonicTime::now();
        m_networkLoadMetrics.markComplete();

        runOnMainThread([this, protectedThis = Ref { *this }, error] {
            if (m_state != State::Canceling && m_client)
                m_client->didCompleteWithError(error, m_networkLoadMetrics);
        });
        return;
    }

    m_networkLoadMetrics.responseEnd = MonotonicTime::now();
    m_networkLoadMetrics.markComplete();

    runOnMainThread([this, protectedThis = Ref { *this }] {
        if (m_state != State::Canceling && m_client)
            m_client->didFinishLoading(m_networkLoadMetrics);
    });
}

bool NetworkDataTaskHaiku::CertificateVerificationFailed(BUrlRequest* caller, BCertificate& certificate, const char* message)
{
    // Check if the user has previously allowed this host
    if (isHTTPSCertificateHostAllowed(m_baseUrl.host().toString()))
        return true;

    // We are in the NetworkProcess, so we cannot easily prompt the user.
    // Ideally, we should notify the UIProcess to ask the user.
    // For now, we log the error and fail securely.
#if !LOG_DISABLED
    LOG(Network, "NetworkDataTaskHaiku Certificate Verification Failed: %s", message);
#endif

    // Notify the client (WebPage) about the failure so it can prompt the user
    // if appropriate (though this path typically ends the request).
    // A proper implementation would pause the request and send an async challenge.
    // But BUrlRequest doesn't support pausing for certificate errors easily.
    // So we fail, and rely on the UI process re-triggering the load after adding exception
    // if the user chooses to proceed (which is handled by DidFailProvisionalLoad in UIProcess).
    return false;
}

void NetworkDataTaskHaiku::didReceiveData(const void* buffer, size_t size)
{
    if (!m_client || m_state == State::Canceling || m_state == State::Completed)
        return;

    if (size == 0) return;

    Vector<uint8_t> dataVector;
    dataVector.append((const uint8_t*)buffer, size);

    runOnMainThread([protectedThis = Ref { *this }, dataVector = WTFMove(dataVector)] {
        if (protectedThis->m_state != State::Canceling && protectedThis->m_client)
            protectedThis->m_client->didReceiveData(SharedBuffer::create(WTFMove(dataVector)));
    });
}

void NetworkDataTaskHaiku::DebugMessage(BUrlRequest* caller, BUrlProtocolDebugMessage type, const char* text)
{
#if !LOG_DISABLED
    switch (type) {
        case B_URL_PROTOCOL_DEBUG_TEXT:
            LOG(Network, "NetworkDataTaskHaiku Debug: %s", text);
            break;
        case B_URL_PROTOCOL_DEBUG_ERROR:
            LOG(Network, "NetworkDataTaskHaiku Error: %s", text);
            break;
        default:
            break;
    }
#endif
}

void NetworkDataTaskHaiku::AuthenticationNeeded(BHttpRequest* request, const ResourceResponse& response)
{
    if (!m_client)
        return;

    m_authFailureCount++;
    if (m_authFailureCount > 3) {
        // Give up after too many tries
        return;
    }

    // Create a basic ProtectionSpace. Haiku BHttpRequest handles auth internally to some degree,
    // but here we are intercepting the failure.
    String authHeader = response.httpHeaderField(HTTPHeaderName::WWWAuthenticate);
    WebCore::ProtectionSpace::AuthenticationScheme scheme = WebCore::ProtectionSpace::AuthenticationScheme::HTTPBasic;
    String realm = "realm"_s;

    if (!authHeader.isEmpty()) {
        if (authHeader.containsIgnoringASCIICase("Digest"))
            scheme = WebCore::ProtectionSpace::AuthenticationScheme::HTTPDigest;

        // Parse realm robustly
        size_t realmPos = authHeader.findIgnoringASCIICase("realm");
        while (realmPos != notFound) {
             // Ensure it is a whole word
             bool precedingCharOk = (realmPos == 0) || authHeader[realmPos - 1] == ' ' || authHeader[realmPos - 1] == '\t' || authHeader[realmPos - 1] == ',';
             if (!precedingCharOk) {
                 realmPos = authHeader.findIgnoringASCIICase("realm", realmPos + 1);
                 continue;
             }

             size_t ptr = realmPos + 5;
             // Skip whitespace
             while (ptr < authHeader.length() && (authHeader[ptr] == ' ' || authHeader[ptr] == '\t'))
                 ptr++;

             if (ptr < authHeader.length() && authHeader[ptr] == '=') {
                 ptr++;
                 // Skip whitespace
                 while (ptr < authHeader.length() && (authHeader[ptr] == ' ' || authHeader[ptr] == '\t'))
                     ptr++;

                 if (ptr < authHeader.length()) {
                     if (authHeader[ptr] == '"') {
                         // Quoted realm
                         ptr++;
                         StringBuilder extractedRealm;
                         while (ptr < authHeader.length()) {
                             UChar c = authHeader[ptr];
                             if (c == '\\' && ptr + 1 < authHeader.length()) {
                                 ptr++;
                                 extractedRealm.append(authHeader[ptr]);
                             } else if (c == '"') {
                                 realm = extractedRealm.toString();
                                 break;
                             } else {
                                 extractedRealm.append(c);
                             }
                             ptr++;
                         }
                         if (!extractedRealm.isEmpty())
                              realm = extractedRealm.toString();
                         break;
                     } else {
                         // Token realm (unquoted)
                         size_t start = ptr;
                         while (ptr < authHeader.length() && authHeader[ptr] != ',' && authHeader[ptr] != ' ' && authHeader[ptr] != '\t')
                             ptr++;
                         realm = authHeader.substring(start, ptr - start);
                         break;
                     }
                 }
             }
             realmPos = authHeader.findIgnoringASCIICase("realm", realmPos + 1);
        }
    }

    WebCore::ProtectionSpace protectionSpace(m_baseUrl.host(), m_baseUrl.port().value_or(0),
        WebCore::ProtectionSpace::ServerType::HTTP, realm, scheme);

    // Using a default ResourceError as previousFailureCount
    AuthenticationChallenge challenge(protectionSpace, Credential(), 0, response, ResourceError());

    runOnMainThread([this, protectedThis = Ref { *this }, challenge = WTFMove(challenge), scheme]() mutable {
        if (m_state == State::Canceling || m_state == State::Completed)
            return;

        if (m_client) {
            m_client->didReceiveAuthenticationChallenge(WTFMove(challenge), NegotiatedLegacyTLS::No, [protectedThis, scheme](AuthenticationChallengeDisposition disposition, const Credential& credential) {
                if (disposition == AuthenticationChallengeDisposition::UseCredential && !credential.isEmpty()) {
                    // Apply credentials to the request logic
                    if (auto* httpRequest = dynamic_cast<BHttpRequest*>(protectedThis->m_request)) {
                        BHttpAuthentication& auth = httpRequest->Authentication();
                        auth.SetUserName(credential.user().utf8().data());
                        auth.SetPassword(credential.password().utf8().data());

                        switch (scheme) {
                        case WebCore::ProtectionSpace::AuthenticationScheme::HTTPDigest:
                            auth.SetMethod(B_HTTP_AUTHENTICATION_DIGEST);
                            break;
                        case WebCore::ProtectionSpace::AuthenticationScheme::HTTPBasic:
                        default:
                            auth.SetMethod(B_HTTP_AUTHENTICATION_BASIC);
                            break;
                        }
                    }
                }
            });
        }
    });
}

}
