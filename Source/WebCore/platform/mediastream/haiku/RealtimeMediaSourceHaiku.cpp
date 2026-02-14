/*
 * Copyright (C) 2024 Haiku, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RealtimeMediaSourceHaiku.h"

#if ENABLE(MEDIA_STREAM)

#include "CaptureDevice.h"
#include "Logging.h"
#include "RealtimeMediaSourceSettings.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

CaptureSourceOrError RealtimeIncomingAudioSourceHaiku::create(const CaptureDevice& device, const MediaConstraints* constraints, String&& hashSalt, std::optional<PageIdentifier> pageIdentifier)
{
    auto source = adoptRef(*new RealtimeIncomingAudioSourceHaiku(device, constraints, WTF::move(hashSalt), pageIdentifier));
    if (constraints) {
        auto result = source->applyConstraints(*constraints);
        if (result)
            return { WTF::move(source), { } };
        return { WTF::move(source), WTF::move(result.value().badConstraint) };
    }
    return { WTF::move(source), { } };
}

RealtimeIncomingAudioSourceHaiku::RealtimeIncomingAudioSourceHaiku(const CaptureDevice& device, const MediaConstraints* constraints, String&& hashSalt, std::optional<PageIdentifier> pageIdentifier)
    : RealtimeMediaSource(device, constraints, WTF::move(hashSalt), pageIdentifier)
{
    // Initialize capabilities/settings
    m_capabilities.setDeviceId(device.persistentId());
    // ... set other capabilities
}

RealtimeIncomingAudioSourceHaiku::~RealtimeIncomingAudioSourceHaiku()
{
}

void RealtimeIncomingAudioSourceHaiku::startProducingData()
{
    // FIXME: Implement audio capture using BMediaRoster
    LOG(Media, "RealtimeIncomingAudioSourceHaiku::startProducingData");
}

void RealtimeIncomingAudioSourceHaiku::stopProducingData()
{
    LOG(Media, "RealtimeIncomingAudioSourceHaiku::stopProducingData");
}

const RealtimeMediaSourceCapabilities& RealtimeIncomingAudioSourceHaiku::capabilities()
{
    return m_capabilities;
}

const RealtimeMediaSourceSettings& RealtimeIncomingAudioSourceHaiku::settings()
{
    return m_settings;
}

// Video

CaptureSourceOrError RealtimeIncomingVideoSourceHaiku::create(const CaptureDevice& device, const MediaConstraints* constraints, String&& hashSalt, std::optional<PageIdentifier> pageIdentifier)
{
    auto source = adoptRef(*new RealtimeIncomingVideoSourceHaiku(device, constraints, WTF::move(hashSalt), pageIdentifier));
    if (constraints) {
        auto result = source->applyConstraints(*constraints);
        if (result)
            return { WTF::move(source), { } };
        return { WTF::move(source), WTF::move(result.value().badConstraint) };
    }
    return { WTF::move(source), { } };
}

RealtimeIncomingVideoSourceHaiku::RealtimeIncomingVideoSourceHaiku(const CaptureDevice& device, const MediaConstraints* constraints, String&& hashSalt, std::optional<PageIdentifier> pageIdentifier)
    : RealtimeMediaSource(device, constraints, WTF::move(hashSalt), pageIdentifier)
{
    m_capabilities.setDeviceId(device.persistentId());
}

RealtimeIncomingVideoSourceHaiku::~RealtimeIncomingVideoSourceHaiku()
{
}

void RealtimeIncomingVideoSourceHaiku::startProducingData()
{
    // FIXME: Implement video capture using BMediaRoster
    LOG(Media, "RealtimeIncomingVideoSourceHaiku::startProducingData");
}

void RealtimeIncomingVideoSourceHaiku::stopProducingData()
{
    LOG(Media, "RealtimeIncomingVideoSourceHaiku::stopProducingData");
}

const RealtimeMediaSourceCapabilities& RealtimeIncomingVideoSourceHaiku::capabilities()
{
    return m_capabilities;
}

const RealtimeMediaSourceSettings& RealtimeIncomingVideoSourceHaiku::settings()
{
    return m_settings;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
