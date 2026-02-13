/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 University of Szeged. All rights reserved.
 * Copyright (C) 2014 Haiku, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS''
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
#include "NetworkStorageSession.h"

#include <support/Locker.h>
#include <UrlContext.h>
#include <UrlProtocolRoster.h>
#include <UrlRequest.h>
#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>
#include <Message.h>

#include "Cookie.h"
#include "CookieRequestHeaderFieldProxy.h"
#include "HTTPCookieAcceptPolicy.h"
#include "NetworkingContext.h"
#include "NotImplemented.h"
#include "ResourceHandle.h"
#include "wtf/URL.h"

#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

#define TRACE_COOKIE_JAR 0

namespace WebCore {

static const char* kCookieDirectory = "WebKit/Cookies";

static void saveCookiesToDisk(BPrivate::Network::BNetworkCookieJar& jar)
{
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
        return;
    path.Append("WebKit");
    create_directory(path.Path(), 0755);
    path.Append("Cookies");

    BFile file(path.Path(), B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE);
    if (file.InitCheck() != B_OK)
        return;

    BMessage msg;
    if (jar.Flatten(&msg) == B_OK)
        msg.Flatten(&file);
}

static void loadCookiesFromDisk(BPrivate::Network::BNetworkCookieJar& jar)
{
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
        return;
    path.Append("WebKit/Cookies");

    BFile file(path.Path(), B_READ_ONLY);
    if (file.InitCheck() != B_OK)
        return;

    BMessage msg;
    if (msg.Unflatten(&file) == B_OK)
        jar.Unflatten(&msg);
}

NetworkStorageSession::NetworkStorageSession(PAL::SessionID sessionID)
    : m_sessionID(sessionID)
    , m_context(nullptr)
{
    if (sessionID.isEphemeral()) {
        m_context = new BPrivate::Network::BUrlContext();
        // Memory only, no persistence
    }
}

NetworkStorageSession::~NetworkStorageSession()
{
    if (m_sessionID.isEphemeral() && m_context) {
        delete m_context;
    }
}

static std::unique_ptr<NetworkStorageSession>& defaultSession()
{
    ASSERT(isMainThread());
    static NeverDestroyed<std::unique_ptr<NetworkStorageSession>> session;
    return session;
}

void NetworkStorageSession::setCookiesFromDOM(const URL& firstParty,
        const SameSiteInfo& sameSiteInfo, const URL& url,
        std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID,
        ApplyTrackingPrevention, const String& value, ShouldRelaxThirdPartyCookieBlocking) const
{
    BPrivate::Network::BNetworkCookie* heapCookie
        = new BPrivate::Network::BNetworkCookie(value, BUrl(url));

    platformSession().GetCookieJar().AddCookie(heapCookie);

    if (!m_sessionID.isEphemeral())
        saveCookiesToDisk(platformSession().GetCookieJar());
}

HTTPCookieAcceptPolicy NetworkStorageSession::cookieAcceptPolicy() const
{
    return HTTPCookieAcceptPolicy::AlwaysAccept;
}

std::pair<String, bool> NetworkStorageSession::cookiesForDOM(const URL& firstParty,
        const SameSiteInfo& sameSiteInfo, const URL& url,
        std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID,
        IncludeSecureCookies includeSecureCookies, ApplyTrackingPrevention,
        ShouldRelaxThirdPartyCookieBlocking) const
{
	BString result;
	BUrl hUrl(url);
	bool secure = false;

	const BPrivate::Network::BNetworkCookie* c;
	for (BPrivate::Network::BNetworkCookieJar::UrlIterator it(
            platformSession().GetCookieJar().GetUrlIterator(hUrl));
		    (c = it.Next()); ) {
        // filter out httpOnly cookies,as this method is used to get cookies
        // from JS code and these shouldn't be visible there.
        if(c->HttpOnly())
			continue;

		// filter out secure cookies if they should be
		if (c->Secure())
		{
			secure = true;
            if (includeSecureCookies == IncludeSecureCookies::No)
				continue;
		}
		
		result << "; " << c->RawCookie(false);
	}

    if (result.Length() > 0)
	    result.Remove(0, 2);

    return {String::fromUTF8(result), secure};
}

void NetworkStorageSession::setCookies(const Vector<Cookie>& cookies, const URL&, const URL&)
{
    for (const auto& cookie : cookies) {
        BPrivate::Network::BNetworkCookie* newCookie = new BPrivate::Network::BNetworkCookie(
            cookie.name.utf8().data(), cookie.value.utf8().data(), BUrl(cookie.domain.utf8().data()));

        newCookie->SetPath(cookie.path.utf8().data());
        newCookie->SetSecure(cookie.secure);
        newCookie->SetHttpOnly(cookie.httpOnly);
        newCookie->SetExpiration(cookie.expires.value_or(0));

        platformSession().GetCookieJar().AddCookie(newCookie);
    }

    if (!m_sessionID.isEphemeral())
        saveCookiesToDisk(platformSession().GetCookieJar());
}

void NetworkStorageSession::setCookie(const Cookie& cookie)
{
    BPrivate::Network::BNetworkCookie* newCookie = new BPrivate::Network::BNetworkCookie(
        cookie.name.utf8().data(), cookie.value.utf8().data(), BUrl(cookie.domain.utf8().data()));

    newCookie->SetPath(cookie.path.utf8().data());
    newCookie->SetSecure(cookie.secure);
    newCookie->SetHttpOnly(cookie.httpOnly);
    newCookie->SetExpiration(cookie.expires.value_or(0));

    platformSession().GetCookieJar().AddCookie(newCookie);

    if (!m_sessionID.isEphemeral())
        saveCookiesToDisk(platformSession().GetCookieJar());
}

void NetworkStorageSession::deleteCookie(const Cookie& cookie, WTF::CompletionHandler<void()>&& completionHandler)
{
    BPrivate::Network::BNetworkCookieJar::Iterator it(platformSession().GetCookieJar().GetIterator());
    const BPrivate::Network::BNetworkCookie* c;

    while ((c = it.Next())) {
        if (c->Name() == cookie.name && c->Domain() == cookie.domain && c->Path() == cookie.path) {
            platformSession().GetCookieJar().RemoveCookie(c);
            break;
        }
    }

    if (!m_sessionID.isEphemeral())
        saveCookiesToDisk(platformSession().GetCookieJar());

    completionHandler();
}

void NetworkStorageSession::deleteCookie(const URL& url, const String& cookieName, WTF::CompletionHandler<void()>&& completionHandler) const
{
#if TRACE_COOKIE_JAR
       printf("CookieJar: delete cookie %s for %s\n", cookieName.utf8().data(), url.string().utf8().data());
#endif
    BPrivate::Network::BNetworkCookieJar::UrlIterator it(platformSession().GetCookieJar().GetUrlIterator(BUrl(url)));
    const BPrivate::Network::BNetworkCookie* c;

    while ((c = it.Next())) {
        if (String::fromUTF8(c->Name()) == cookieName) {
            platformSession().GetCookieJar().RemoveCookie(c);
            break;
        }
    }

    if (!m_sessionID.isEphemeral())
        saveCookiesToDisk(platformSession().GetCookieJar());

    completionHandler();
}

void NetworkStorageSession::deleteAllCookies(WTF::CompletionHandler<void()>&& completionHandler)
{
    platformSession().GetCookieJar().Purge(NULL);

    if (!m_sessionID.isEphemeral())
        saveCookiesToDisk(platformSession().GetCookieJar());

    completionHandler();
}

void NetworkStorageSession::deleteAllCookiesModifiedSince(WallTime since, WTF::CompletionHandler<void()>&& completionHandler)
{
    BPrivate::Network::BNetworkCookieJar::Iterator it(platformSession().GetCookieJar().GetIterator());
    const BPrivate::Network::BNetworkCookie* c;
    Vector<const BPrivate::Network::BNetworkCookie*> cookiesToRemove;

    time_t sinceTime = static_cast<time_t>(since.secondsSinceEpoch().seconds());

    while ((c = it.Next())) {
        if (c->LastAccessTime() >= sinceTime || c->CreationTime() >= sinceTime) {
            cookiesToRemove.append(c);
        }
    }

    for (auto* cookie : cookiesToRemove) {
        platformSession().GetCookieJar().RemoveCookie(cookie);
    }

    if (!m_sessionID.isEphemeral())
        saveCookiesToDisk(platformSession().GetCookieJar());

    completionHandler();
}

void NetworkStorageSession::deleteCookiesForHostnames(const Vector<String>& cookieHostNames,
    WebCore::IncludeHttpOnlyCookies includeHttpOnly, WebCore::ScriptWrittenCookiesOnly, WTF::CompletionHandler<void()>&& completionHandler)
{
    BPrivate::Network::BNetworkCookieJar::Iterator it(platformSession().GetCookieJar().GetIterator());
    const BPrivate::Network::BNetworkCookie* c;

    Vector<const BPrivate::Network::BNetworkCookie*> cookiesToRemove;

    while ((c = it.Next())) {
        if (cookieHostNames.contains(String::fromUTF8(c->Domain()))) {
             if (c->HttpOnly() && includeHttpOnly == IncludeHttpOnlyCookies::No)
                continue;
            cookiesToRemove.append(c);
        }
    }

    for (auto* cookie : cookiesToRemove) {
        platformSession().GetCookieJar().RemoveCookie(cookie);
    }

    if (!m_sessionID.isEphemeral())
        saveCookiesToDisk(platformSession().GetCookieJar());

    completionHandler();
}

Vector<Cookie> NetworkStorageSession::getAllCookies()
{
    Vector<Cookie> cookies;
    BPrivate::Network::BNetworkCookieJar::Iterator it(platformSession().GetCookieJar().GetIterator());
    const BPrivate::Network::BNetworkCookie* c;

    while ((c = it.Next())) {
        Cookie cookie;
        cookie.name = String::fromUTF8(c->Name());
        cookie.value = String::fromUTF8(c->Value());
        cookie.domain = String::fromUTF8(c->Domain());
        cookie.path = String::fromUTF8(c->Path());
        cookie.secure = c->Secure();
        cookie.httpOnly = c->HttpOnly();
        cookie.expires = c->Expiration();
        cookies.append(cookie);
    }
    return cookies;
}

void NetworkStorageSession::getHostnamesWithCookies(HashSet<String>& hostnames)
{
    BPrivate::Network::BNetworkCookieJar::Iterator it(platformSession().GetCookieJar().GetIterator());
    const BPrivate::Network::BNetworkCookie* c;

    while ((c = it.Next())) {
        hostnames.add(String::fromUTF8(c->Domain()));
    }
}

Vector<Cookie> NetworkStorageSession::getCookies(const URL& url)
{
    Vector<Cookie> cookies;
    BUrl hUrl(url);
    BPrivate::Network::BNetworkCookieJar::UrlIterator it(platformSession().GetCookieJar().GetUrlIterator(hUrl));
    const BPrivate::Network::BNetworkCookie* c;

    while ((c = it.Next())) {
        Cookie cookie;
        cookie.name = String::fromUTF8(c->Name());
        cookie.value = String::fromUTF8(c->Value());
        cookie.domain = String::fromUTF8(c->Domain());
        cookie.path = String::fromUTF8(c->Path());
        cookie.secure = c->Secure();
        cookie.httpOnly = c->HttpOnly();
        cookie.expires = c->Expiration();
        cookies.append(cookie);
    }
    return cookies;
}

void NetworkStorageSession::hasCookies(const RegistrableDomain& domain, CompletionHandler<void(bool)>&& completionHandler) const
{
    BPrivate::Network::BNetworkCookieJar::Iterator it(platformSession().GetCookieJar().GetIterator());
    const BPrivate::Network::BNetworkCookie* c;
    bool found = false;
    while ((c = it.Next())) {
        if (String::fromUTF8(c->Domain()).endsWith(domain.string())) {
            found = true;
            break;
        }
    }
    completionHandler(found);
}

bool NetworkStorageSession::getRawCookies(const URL& firstParty,
	const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID,
	std::optional<PageIdentifier> pageID, ApplyTrackingPrevention, ShouldRelaxThirdPartyCookieBlocking, Vector<Cookie>& rawCookies) const
{
    rawCookies.clear();

    BUrl hUrl(url);
    BPrivate::Network::BNetworkCookieJar& jar = platformSession().GetCookieJar();
    BPrivate::Network::BNetworkCookieJar::UrlIterator it(jar.GetUrlIterator(hUrl));
    const BPrivate::Network::BNetworkCookie* c;

    while ((c = it.Next())) {
        rawCookies.append(Cookie(
            String::fromUTF8(c->Name()),
            String::fromUTF8(c->Value()),
            String::fromUTF8(c->Domain()),
            String::fromUTF8(c->Path()),
            (double)c->CreationTime(), (double)c->ExpirationDate(), (double)c->LastAccessTime(),
            c->HttpOnly(),
            c->Secure(),
            c->ExpirationDate() == 0
        ));
    }

    return true;
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(const URL& firstParty,
	const SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID,
	std::optional<PageIdentifier> pageID, IncludeSecureCookies includeSecureCookies, ApplyTrackingPrevention,
	ShouldRelaxThirdPartyCookieBlocking) const
{
	BString result;
	BUrl hUrl(url);
	bool secure = false;

	const BPrivate::Network::BNetworkCookie* c;
	for (BPrivate::Network::BNetworkCookieJar::UrlIterator it(
        	platformSession().GetCookieJar().GetUrlIterator(hUrl));
		    (c = it.Next()); ) {
		// filter out secure cookies if they should be
		if (c->Secure())
		{
			secure = true;
            if (includeSecureCookies == IncludeSecureCookies::No)
				continue;
		}
		
		result << "; " << c->RawCookie(false);
	}

    if (result.Length() > 0)
	    result.Remove(0, 2);

    return {String::fromUTF8(result.String()), secure};
}

std::pair<String, bool> NetworkStorageSession::cookieRequestHeaderFieldValue(
    const CookieRequestHeaderFieldProxy& headerFieldProxy) const
{
    return cookieRequestHeaderFieldValue(headerFieldProxy.firstParty,
        headerFieldProxy.sameSiteInfo, headerFieldProxy.url,
        headerFieldProxy.frameID, headerFieldProxy.pageID,
        headerFieldProxy.includeSecureCookies, ApplyTrackingPrevention::Yes,
        ShouldRelaxThirdPartyCookieBlocking::No);
}

BPrivate::Network::BUrlContext& NetworkStorageSession::platformSession() const
{
    if (m_context)
        return *m_context;

    static BPrivate::Network::BUrlContext* sDefaultContext = nullptr;

    if (sDefaultContext == nullptr) {
        BPrivate::Network::BUrlRequest* fakeRequest
            = BPrivate::Network::BUrlProtocolRoster::MakeRequest(BUrl("data:"), NULL, NULL);
        sDefaultContext = fakeRequest->Context();
        delete fakeRequest;

        loadCookiesFromDisk(sDefaultContext->GetCookieJar());
    }
    return *sDefaultContext;
}

void NetworkStorageSession::setPlatformSession(BPrivate::Network::BUrlContext* context)
{
    m_context = context;
}

}
