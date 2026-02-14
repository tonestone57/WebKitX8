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
#include "RealtimeMediaSourceCenter.h"

#if ENABLE(MEDIA_STREAM)

#include "AudioCaptureFactory.h"
#include "VideoCaptureFactory.h"
#include "DisplayCaptureFactory.h"
#include "RealtimeMediaSourceHaiku.h"
#include "CaptureDeviceManager.h"
#include "DisplayCaptureManager.h"
#include "Logging.h"
#include <MediaRoster.h>
#include <MediaNode.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

class CaptureDeviceManagerHaiku final : public CaptureDeviceManager {
public:
    CaptureDeviceManagerHaiku(CaptureDevice::DeviceType type) : m_type(type) {}

    const Vector<CaptureDevice>& captureDevices() final { return m_devices; }

    void computeCaptureDevices(CompletionHandler<void()>&& completion) final {
        m_devices.clear();

        BMediaRoster* roster = BMediaRoster::Roster();
        if (!roster) {
            completion();
            return;
        }

        live_node_info liveNodes[20];
        int32 count = 20;
        uint64 nodeType = B_BUFFER_PRODUCER | B_PHYSICAL_INPUT;
        media_format format;
        memset(&format, 0, sizeof(format));

        if (m_type == CaptureDevice::DeviceType::Microphone) {
            format.type = B_MEDIA_RAW_AUDIO;
        } else if (m_type == CaptureDevice::DeviceType::Camera) {
            format.type = B_MEDIA_RAW_VIDEO;
        } else {
             completion();
             return;
        }

        if (roster->GetLiveNodes(liveNodes, &count, NULL, &format, nodeType, 0) == B_OK) {
            for (int32 i = 0; i < count; i++) {
                String persistentId = String::number(liveNodes[i].node.node);
                String label = String::fromUTF8(liveNodes[i].name);
                m_devices.append(CaptureDevice(persistentId, m_type, label, persistentId, true, false, false));
            }
        }

        completion();
    }

private:
    CaptureDevice::DeviceType m_type;
    Vector<CaptureDevice> m_devices;
};

class AudioCaptureFactoryHaiku final : public AudioCaptureFactory {
public:
    CaptureSourceOrError createAudioCaptureSource(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, std::optional<PageIdentifier> pageIdentifier) final
    {
        return RealtimeIncomingAudioSourceHaiku::create(device, constraints, WTF::move(hashSalts.persistentDeviceIDHashSalt), pageIdentifier);
    }

    CaptureDeviceManager& audioCaptureDeviceManager() final
    {
        static NeverDestroyed<CaptureDeviceManagerHaiku> manager(CaptureDevice::DeviceType::Microphone);
        return manager;
    }

    const Vector<CaptureDevice>& speakerDevices() const final
    {
        static const Vector<CaptureDevice> devices;
        return devices;
    }
};

class VideoCaptureFactoryHaiku final : public VideoCaptureFactory {
public:
    CaptureSourceOrError createVideoCaptureSource(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, std::optional<PageIdentifier> pageIdentifier) final
    {
        return RealtimeIncomingVideoSourceHaiku::create(device, constraints, WTF::move(hashSalts.persistentDeviceIDHashSalt), pageIdentifier);
    }

    CaptureDeviceManager& videoCaptureDeviceManager() final
    {
        static NeverDestroyed<CaptureDeviceManagerHaiku> manager(CaptureDevice::DeviceType::Camera);
        return manager;
    }
};

class DisplayCaptureFactoryHaiku final : public DisplayCaptureFactory {
public:
    CaptureSourceOrError createDisplayCaptureSource(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, std::optional<PageIdentifier> pageIdentifier) final
    {
        return { "Not implemented"_s };
    }

    DisplayCaptureManager& displayCaptureDeviceManager() final
    {
        static NeverDestroyed<DisplayCaptureManager> manager;
        return manager;
    }
};

AudioCaptureFactory& RealtimeMediaSourceCenter::defaultAudioCaptureFactory()
{
    static NeverDestroyed<AudioCaptureFactoryHaiku> factory;
    return factory;
}

VideoCaptureFactory& RealtimeMediaSourceCenter::defaultVideoCaptureFactory()
{
    static NeverDestroyed<VideoCaptureFactoryHaiku> factory;
    return factory;
}

DisplayCaptureFactory& RealtimeMediaSourceCenter::defaultDisplayCaptureFactory()
{
    static NeverDestroyed<DisplayCaptureFactoryHaiku> factory;
    return factory;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
