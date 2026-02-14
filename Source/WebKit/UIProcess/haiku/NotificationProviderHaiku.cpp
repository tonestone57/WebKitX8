/*
 * Copyright (C) 2024 Haiku, Inc. All rights reserved.
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
#include "NotificationProviderHaiku.h"

#include "WebNotification.h"
#include "WebPageProxy.h"
#include <WebCore/NotificationData.h>
#include <WebCore/NotificationResources.h>

#include <Notification.h>
#include <String.h>

namespace WebKit {

NotificationProviderHaiku::NotificationProviderHaiku()
{
}

NotificationProviderHaiku::~NotificationProviderHaiku()
{
}

bool NotificationProviderHaiku::show(WebPageProxy* page, WebNotification& webNotification, RefPtr<WebCore::NotificationResources>&& resources)
{
    BNotification notification(B_INFORMATION_NOTIFICATION);
    notification.SetTitle(webNotification.title().utf8().data());
    notification.SetContent(webNotification.body().utf8().data());
    notification.SetMessageID(webNotification.coreNotificationID().toString().utf8().data());

    notification.Send();

    return true;
}

void NotificationProviderHaiku::cancel(WebNotification& webNotification)
{
    // Haiku BNotification currently does not support programmatic cancellation via ID easily.
}

void NotificationProviderHaiku::didDestroyNotification(WebNotification&)
{
}

void NotificationProviderHaiku::clearNotifications(const Vector<WebNotificationIdentifier>&)
{
}

HashMap<WTF::String, bool> NotificationProviderHaiku::notificationPermissions()
{
    return { };
}

} // namespace WebKit
