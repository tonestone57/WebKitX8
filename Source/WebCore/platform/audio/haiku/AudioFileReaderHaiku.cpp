/*
 *  Copyright (C) 2014 Haiku, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "AudioFileReader.h"

#include "AudioBus.h"
#include "NotImplemented.h"

#include <File.h>
#include <MediaFile.h>
#include <MediaTrack.h>

namespace WebCore {

class AudioFileReader {
    WTF_MAKE_NONCOPYABLE(AudioFileReader);
public:
    AudioFileReader(const char* filePath);
    AudioFileReader(std::span<const uint8_t> data);
    ~AudioFileReader();

    RefPtr<AudioBus> createBus(float sampleRate, bool mixToMono);

private:
    BDataIO* m_data;
    BMediaFile* m_file;
};

AudioFileReader::AudioFileReader(const char* filePath)
{
    m_data = new BFile(filePath, B_READ_ONLY);
    m_file = new BMediaFile(m_data);
}

AudioFileReader::AudioFileReader(std::span<const uint8_t> data)
{
    m_data = new BMemoryIO(data.data(), data.size());
    m_file = new BMediaFile(m_data);
}

AudioFileReader::~AudioFileReader()
{
    delete m_file;
    delete m_data;
}

RefPtr<AudioBus> AudioFileReader::createBus(float sampleRate, bool mixToMono)
{
    BMediaTrack* track = m_file->TrackAt(0);

    if (track && track->InitCheck() == B_OK) {
        media_format format;
        if (track->EncodedFormat(&format) != B_OK) {
            m_file->ReleaseTrack(track);
            return AudioBus::create(0, 0, true);
        }

        // Setup format conversion
        memset(&format, 0, sizeof(media_format));
        format.type = B_MEDIA_RAW_AUDIO;
        format.u.raw_audio.format = B_AUDIO_FLOAT;
        format.u.raw_audio.byte_order = B_MEDIA_HOST_ENDIAN;
        format.u.raw_audio.frame_rate = sampleRate;
        format.u.raw_audio.channel_count = mixToMono ? 1 : 2;

        if (track->SetDecodedFormat(&format) != B_OK) {
            m_file->ReleaseTrack(track);
            return AudioBus::create(0, 0, true);
        }

        int64 frames = track->CountFrames();
        if (frames <= 0) {
            m_file->ReleaseTrack(track);
            return AudioBus::create(0, 0, true);
        }

        unsigned channels = format.u.raw_audio.channel_count;
        Ref<AudioBus> audioBus = AudioBus::create(channels, frames, true);
        audioBus->setSampleRate(sampleRate);

        // Get pointers to the audio bus channels
        Vector<float*> channelData;
        for (unsigned i = 0; i < channels; ++i)
            channelData.append(audioBus->channel(i)->mutableData());

        // Read and deinterleave
        const int bufferFrameCount = 4096;
        float* interleavedBuffer = new float[bufferFrameCount * channels];
        int64 framesRead = 0;
        int64 totalFrames = 0;

        while (totalFrames < frames) {
            int64 currentRequest = bufferFrameCount;
            if (track->ReadFrames(interleavedBuffer, &currentRequest) != B_OK)
                break;

            if (currentRequest <= 0)
                break;

            for (int i = 0; i < currentRequest; ++i) {
                for (unsigned c = 0; c < channels; ++c) {
                    if (totalFrames + i < frames)
                        channelData[c][totalFrames + i] = interleavedBuffer[i * channels + c];
                }
            }
            totalFrames += currentRequest;
        }

        delete[] interleavedBuffer;
        m_file->ReleaseTrack(track);
        return audioBus;
    } else {
        // Reading the file failed, return an empty bus
        if (track) m_file->ReleaseTrack(track);
        Ref<AudioBus> audioBus = AudioBus::create(0, 0, true);
        return audioBus;
    }
}

RefPtr<AudioBus> createBusFromAudioFile(const char* filePath, bool mixToMono, float sampleRate)
{
    return AudioFileReader(filePath).createBus(sampleRate, mixToMono);
}

RefPtr<AudioBus> createBusFromInMemoryAudioFile(std::span<const uint8_t> data, bool mixToMono, float sampleRate)
{
    return AudioFileReader(data).createBus(sampleRate, mixToMono);
}

} // WebCore

#endif // ENABLE(WEB_AUDIO)
