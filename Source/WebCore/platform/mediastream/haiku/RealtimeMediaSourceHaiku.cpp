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
#include <MediaRoster.h>
#include <Buffer.h>
#include <BufferConsumer.h>
#include <MediaEventLooper.h>
#include <wtf/NeverDestroyed.h>
#include <cstring>
#include <span>

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

// Custom BBufferConsumer to capture data
class MediaCaptureNode : public BBufferConsumer {
public:
    using Callback = Function<void(BBuffer*)>;

    MediaCaptureNode(const char* name, media_type type, Callback&& callback)
        : BBufferConsumer(B_MEDIA_RAW_AUDIO) // flavor doesn't matter much here
        , m_name(name)
        , m_type(type)
        , m_callback(WTF::move(callback))
        , m_input()
    {
        // Must be registered to work
    }

    ~MediaCaptureNode()
    {
        Quit();
    }

    status_t Start(bigtime_t performance_time) override
    {
        return BBufferConsumer::Start(performance_time);
    }

    status_t Stop(bigtime_t performance_time, bool immediate) override
    {
        return BBufferConsumer::Stop(performance_time, immediate);
    }

    void BufferReceived(BBuffer* buffer) override
    {
        if (m_callback)
            m_callback(buffer);
        else
            buffer->Recycle();
    }

    status_t AcceptFormat(const media_destination& dest, media_format* format) override
    {
        if (format->type != m_type && format->type != B_MEDIA_WILDCARD)
            return B_MEDIA_BAD_FORMAT;
        return B_OK;
    }

    status_t Connected(const media_source& producer, const media_destination& where, const media_format& with_format, media_input* out_input) override
    {
        m_input.source = producer;
        m_input.destination = where;
        m_input.format = with_format;
        m_input.node = Node();
        sprintf(m_input.name, "%s input", m_name.utf8().data());
        *out_input = m_input;
        return B_OK;
    }

    void Disconnected(const media_source& producer, const media_destination& where) override
    {
        memset(&m_input, 0, sizeof(m_input));
    }

    status_t GetNextInput(int32* cookie, media_input* out_input) override
    {
        if (*cookie != 0) return B_ERROR;
        *out_input = m_input;
        *cookie = 1;
        return B_OK;
    }

    void DisposeInputCookie(int32 cookie) override {}

    media_input Input() { return m_input; }

private:
    String m_name;
    media_type m_type;
    Callback m_callback;
    media_input m_input;
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
    , m_node(nullptr)
    , m_isCapturing(false)
{
    m_capabilities.setDeviceId(device.persistentId());
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
        captureFailed();
        return;
    }

    BMediaRoster* roster = BMediaRoster::Roster();
    if (!roster) {
        captureFailed();
        return;
    }

    media_node producerNode;
    if (roster->GetNodeFor(nodeId, &producerNode) != B_OK) {
        captureFailed();
        return;
    }

    m_node = new MediaCaptureNode("WebAudio Capture", B_MEDIA_RAW_AUDIO, [this](BBuffer* buffer) {
        handleBuffer(buffer);
    });

    if (roster->RegisterNode(m_node) != B_OK) {
        delete m_node;
        m_node = nullptr;
        captureFailed();
        return;
    }

    // Connect
    media_output output;
    int32 count = 0;
    if (roster->GetFreeOutputsFor(producerNode, &output, 1, &count, B_MEDIA_RAW_AUDIO) != B_OK || count == 0) {
        stopProducingData();
        captureFailed();
        return;
    }

    media_input input;
    count = 0;
    if (roster->GetFreeInputsFor(m_node->Node(), &input, 1, &count, B_MEDIA_RAW_AUDIO) != B_OK || count == 0) {
        stopProducingData();
        captureFailed();
        return;
    }

    media_format format;
    format.type = B_MEDIA_RAW_AUDIO;
    format.u.raw_audio = media_raw_audio_format::wildcard;
    format.u.raw_audio.format = media_raw_audio_format::B_AUDIO_FLOAT;

    if (roster->Connect(output.source, input.destination, &format, &output, &input) != B_OK) {
        stopProducingData();
        captureFailed();
        return;
    }

    roster->StartNode(m_node->Node(), 0);
    roster->StartNode(producerNode, 0);

    m_isCapturing = true;
}

void RealtimeIncomingAudioSourceHaiku::stopProducingData()
{
    if (m_node) {
        BMediaRoster* roster = BMediaRoster::Roster();
        if (roster) {
            roster->StopNode(m_node->Node(), 0);

            // Disconnect
            media_input input = m_node->Input();
            if (input.source != media_source::null) {
                roster->Disconnect(input.source, input.destination);
            }

            roster->UnregisterNode(m_node);
        }
        m_node->Release(); // BMediaNode is refcounted, Release() calls delete when done
        m_node = nullptr;
    }
    m_isCapturing = false;
}

void RealtimeIncomingAudioSourceHaiku::handleBuffer(BBuffer* buffer)
{
     if (!m_isCapturing) {
         buffer->Recycle();
         return;
     }

     media_header* header = buffer->Header();
     if (header->type == B_MEDIA_RAW_AUDIO) {
         media_raw_audio_format* format = &header->u.raw_audio;

         AudioStreamDescription description(format->frame_rate, format->channel_count, AudioStreamDescription::PCMFormat::Float32, true);

         size_t frameSize = (format->format & 0xf) * format->channel_count;
         if (format->format == media_raw_audio_format::B_AUDIO_FLOAT) frameSize = 4 * format->channel_count;
         else if (format->format == media_raw_audio_format::B_AUDIO_SHORT) frameSize = 2 * format->channel_count;

         size_t numFrames = buffer->SizeUsed() / frameSize;

         auto audioData = PlatformAudioDataHaiku::create((const uint8_t*)buffer->Data(), buffer->SizeUsed());
         MediaTime timestamp = MediaTime(header->start_time, 1000000);

         audioSamplesAvailable(timestamp, audioData, description, numFrames);
     }
     buffer->Recycle();
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
    , m_node(nullptr)
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

    BMediaRoster* roster = BMediaRoster::Roster();
    if (!roster) {
        captureFailed();
        return;
    }

    media_node producerNode;
    if (roster->GetNodeFor(nodeId, &producerNode) != B_OK) {
        captureFailed();
        return;
    }

    m_node = new MediaCaptureNode("WebVideo Capture", B_MEDIA_RAW_VIDEO, [this](BBuffer* buffer) {
        handleBuffer(buffer);
    });

    if (roster->RegisterNode(m_node) != B_OK) {
        delete m_node;
        m_node = nullptr;
        captureFailed();
        return;
    }

    // Connect
    media_output output;
    int32 count = 0;
    if (roster->GetFreeOutputsFor(producerNode, &output, 1, &count, B_MEDIA_RAW_VIDEO) != B_OK || count == 0) {
        stopProducingData();
        captureFailed();
        return;
    }

    media_input input;
    count = 0;
    if (roster->GetFreeInputsFor(m_node->Node(), &input, 1, &count, B_MEDIA_RAW_VIDEO) != B_OK || count == 0) {
        stopProducingData();
        captureFailed();
        return;
    }

    media_format format;
    format.type = B_MEDIA_RAW_VIDEO;
    format.u.raw_video = media_raw_video_format::wildcard;
    format.u.raw_video.display.format = B_RGB32;

    if (roster->Connect(output.source, input.destination, &format, &output, &input) != B_OK) {
        stopProducingData();
        captureFailed();
        return;
    }

    roster->StartNode(m_node->Node(), 0);
    roster->StartNode(producerNode, 0);

    m_isCapturing = true;
}

void RealtimeIncomingVideoSourceHaiku::stopProducingData()
{
    if (m_node) {
        BMediaRoster* roster = BMediaRoster::Roster();
        if (roster) {
            roster->StopNode(m_node->Node(), 0);

            media_input input = m_node->Input();
            if (input.source != media_source::null) {
                roster->Disconnect(input.source, input.destination);
            }
            roster->UnregisterNode(m_node);
        }
        m_node->Release();
        m_node = nullptr;
    }
    m_isCapturing = false;
}

void RealtimeIncomingVideoSourceHaiku::handleBuffer(BBuffer* buffer)
{
     if (!m_isCapturing) {
         buffer->Recycle();
         return;
     }

     media_header* header = buffer->Header();
     if (header->type == B_MEDIA_RAW_VIDEO) {
         media_raw_video_format* format = &header->u.raw_video;

         int width = format->display.line_width;
         int height = format->display.line_count;

         if (width > 0 && height > 0) {
             ComputedPlaneLayout layout;
             layout.destinationOffset = 0;
             layout.destinationStride = format->display.bytes_per_row;
             layout.sourceTop = 0;
             layout.sourceHeight = height;
             layout.sourceLeftBytes = 0;
             layout.sourceWidthBytes = width * 4;

             PlatformVideoColorSpace colorSpace;

             auto frame = VideoFrame::createBGRA(
                 std::span<const uint8_t>((const uint8_t*)buffer->Data(), buffer->SizeUsed()),
                 width, height, layout, WTF::move(colorSpace)
             );

             if (frame) {
                 VideoFrameTimeMetadata metadata;
                 metadata.captureTime = MonotonicTime::now();

                 videoFrameAvailable(*frame, metadata);
             }
         }
     }
     buffer->Recycle();
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
