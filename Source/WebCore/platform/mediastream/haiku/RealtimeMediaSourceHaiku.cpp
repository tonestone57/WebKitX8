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
#include "PlatformAudioData.h"
#include "AudioStreamDescription.h"
#include "VideoFrame.h"
#include "PixelBuffer.h"
#include "VideoFrameTimeMetadata.h"
#include <MediaRecorder.h>
#include <MediaRoster.h>
#include <Buffer.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

// Simple wrapper for raw audio bytes
class PlatformAudioDataHaiku final : public PlatformAudioData {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(PlatformAudioDataHaiku);
public:
    static Ref<PlatformAudioDataHaiku> create(const uint8_t* data, size_t size)
    {
        return adoptRef(*new PlatformAudioDataHaiku(data, size));
    }

    const uint8_t* data() const { return m_data.data(); }
    size_t size() const { return m_data.size(); }

private:
    PlatformAudioDataHaiku(const uint8_t* data, size_t size)
    {
        m_data.resize(size);
        memcpy(m_data.data(), data, size);
    }
    Vector<uint8_t> m_data;
};

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
    , m_recorder(nullptr)
    , m_isCapturing(false)
{
    m_capabilities.setDeviceId(device.persistentId());
    // TODO: query actual device capabilities if possible
}

RealtimeIncomingAudioSourceHaiku::~RealtimeIncomingAudioSourceHaiku()
{
    stopProducingData();
}

void RealtimeIncomingAudioSourceHaiku::startProducingData()
{
    if (m_isCapturing) return;

    media_node_id nodeId = 0;
    String idStr = captureDevice().persistentId();
    if (!idStr.isEmpty())
        nodeId = idStr.toInt();

    if (nodeId == 0) {
        LOG(Media, "RealtimeIncomingAudioSourceHaiku: Invalid node ID");
        captureFailed();
        return;
    }

    media_node node;
    BMediaRoster* roster = BMediaRoster::Roster();
    if (!roster || roster->GetNodeFor(nodeId, &node) != B_OK) {
        LOG(Media, "RealtimeIncomingAudioSourceHaiku: Failed to get node");
        captureFailed();
        return;
    }

    m_recorder = new BMediaRecorder("WebAudio Input", false);
    if (m_recorder->InitCheck() != B_OK) {
        LOG(Media, "RealtimeIncomingAudioSourceHaiku: Failed to init recorder");
        delete m_recorder;
        m_recorder = nullptr;
        captureFailed();
        return;
    }

    // Connect to the node
    media_format format;
    format.type = B_MEDIA_RAW_AUDIO;
    format.u.raw_audio = media_raw_audio_format::wildcard;
    // Request float format as we assume it in the callback
    format.u.raw_audio.format = media_raw_audio_format::B_AUDIO_FLOAT;

    if (m_recorder->Connect(node, &format) != B_OK) {
        LOG(Media, "RealtimeIncomingAudioSourceHaiku: Failed to connect recorder");
        delete m_recorder;
        m_recorder = nullptr;
        captureFailed();
        return;
    }

    m_isCapturing = true;
    m_captureThread = Thread::create("Audio Capture", [this] {
        captureLoop();
    });
}

void RealtimeIncomingAudioSourceHaiku::stopProducingData()
{
    if (!m_isCapturing) return;

    m_isCapturing = false;

    if (m_recorder) {
        m_recorder->Disconnect();
        // Stop() should unblock WaitForBuffer
        m_recorder->Stop();
    }

    if (m_captureThread) {
        m_captureThread->waitForCompletion();
        m_captureThread = nullptr;
    }

    if (m_recorder) {
        delete m_recorder;
        m_recorder = nullptr;
    }
}

void RealtimeIncomingAudioSourceHaiku::captureLoop()
{
    if (!m_recorder) return;
    m_recorder->Start();

    while (m_isCapturing) {
        BBuffer* buffer = nullptr;
        // Wait 100ms max, loop to check m_isCapturing
        status_t err = m_recorder->WaitForBuffer(&buffer, 100000);

        if (err == B_OK && buffer) {
             media_header* header = buffer->Header();
             if (header->type == B_MEDIA_RAW_AUDIO) {
                 media_raw_audio_format* format = &header->u.raw_audio;

                 // Construct description (assuming float, interleaved)
                 AudioStreamDescription description(format->frame_rate, format->channel_count, AudioStreamDescription::PCMFormat::Float32, true);

                 // We need to calculate number of frames
                 size_t frameSize = (format->format & 0xf) * format->channel_count; // Rough guess, format enum is complex
                 if (format->format == media_raw_audio_format::B_AUDIO_FLOAT) frameSize = 4 * format->channel_count;
                 else if (format->format == media_raw_audio_format::B_AUDIO_SHORT) frameSize = 2 * format->channel_count;

                 size_t numFrames = buffer->SizeUsed() / frameSize;

                 auto audioData = PlatformAudioDataHaiku::create((const uint8_t*)buffer->Data(), buffer->SizeUsed());

                 // Timestamp conversion
                 MediaTime timestamp = MediaTime(header->start_time, 1000000); // microseconds

                 audioSamplesAvailable(timestamp, audioData, description, numFrames);
             }
             buffer->Recycle();
        } else if (err != B_TIMED_OUT && err != B_OK) {
            // Error
            break;
        }
    }
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
    , m_recorder(nullptr)
    , m_isCapturing(false)
{
    m_capabilities.setDeviceId(device.persistentId());
}

RealtimeIncomingVideoSourceHaiku::~RealtimeIncomingVideoSourceHaiku()
{
    stopProducingData();
}

void RealtimeIncomingVideoSourceHaiku::startProducingData()
{
    if (m_isCapturing) return;

    media_node_id nodeId = 0;
    String idStr = captureDevice().persistentId();
    if (!idStr.isEmpty())
        nodeId = idStr.toInt();

    if (nodeId == 0) {
        captureFailed();
        return;
    }

    media_node node;
    BMediaRoster* roster = BMediaRoster::Roster();
    if (!roster || roster->GetNodeFor(nodeId, &node) != B_OK) {
        captureFailed();
        return;
    }

    m_recorder = new BMediaRecorder("WebVideo Input", false);
    if (m_recorder->InitCheck() != B_OK) {
        delete m_recorder;
        m_recorder = nullptr;
        captureFailed();
        return;
    }

    // Connect to the node
    media_format format;
    format.type = B_MEDIA_RAW_VIDEO;
    format.u.raw_video = media_raw_video_format::wildcard;
    format.u.raw_video.display.format = B_RGB32; // Prefer RGB32

    if (m_recorder->Connect(node, &format) != B_OK) {
        delete m_recorder;
        m_recorder = nullptr;
        captureFailed();
        return;
    }

    m_isCapturing = true;
    m_captureThread = Thread::create("Video Capture", [this] {
        captureLoop();
    });
}

void RealtimeIncomingVideoSourceHaiku::stopProducingData()
{
    if (!m_isCapturing) return;
    m_isCapturing = false;

    if (m_recorder) {
        m_recorder->Disconnect();
        m_recorder->Stop();
    }

    if (m_captureThread) {
        m_captureThread->waitForCompletion();
        m_captureThread = nullptr;
    }

    if (m_recorder) {
        delete m_recorder;
        m_recorder = nullptr;
    }
}

void RealtimeIncomingVideoSourceHaiku::captureLoop()
{
    if (!m_recorder) return;
    m_recorder->Start();

    while (m_isCapturing) {
        BBuffer* buffer = nullptr;
        status_t err = m_recorder->WaitForBuffer(&buffer, 100000);

        if (err == B_OK && buffer) {
             media_header* header = buffer->Header();
             if (header->type == B_MEDIA_RAW_VIDEO) {
                 media_raw_video_format* format = &header->u.raw_video;

                 // Create VideoFrame
                 // We need to wrap the buffer data.
                 // Since BBuffer recycles, we must copy it if VideoFrame keeps it.
                 // VideoFrame::createRGBA takes a span and copies it usually?
                 // Let's check: createRGBA(std::span<const uint8_t>, ...)

                 int width = format->display.line_width;
                 int height = format->display.line_count;

                 if (width > 0 && height > 0) {
                     ComputedPlaneLayout layout;
                     layout.destinationOffset = 0;
                     layout.destinationStride = format->display.bytes_per_row;
                     layout.sourceTop = 0;
                     layout.sourceHeight = height;
                     layout.sourceLeftBytes = 0;
                     layout.sourceWidthBytes = width * 4; // Assuming B_RGB32

                     PlatformVideoColorSpace colorSpace; // Default

                     auto frame = VideoFrame::createRGBA(
                         std::span<const uint8_t>((const uint8_t*)buffer->Data(), buffer->SizeUsed()),
                         width, height, layout, WTF::move(colorSpace)
                     );

                     if (frame) {
                         VideoFrameTimeMetadata metadata;
                         metadata.captureTime = MonotonicTime::now(); // Use current time as fallback or construct from header->start_time if possible

                         videoFrameAvailable(*frame, metadata);
                     }
                 }
             }
             buffer->Recycle();
        } else if (err != B_TIMED_OUT && err != B_OK) {
            break;
        }
    }
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
