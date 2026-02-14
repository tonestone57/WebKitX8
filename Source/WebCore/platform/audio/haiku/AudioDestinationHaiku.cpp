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

#include "AudioDestinationHaiku.h"

#include "AudioIOCallback.h"
#include "Logging.h"
#include <SoundPlayer.h>
#include <OS.h>
#include <string.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Seconds.h>

namespace WebCore {

std::unique_ptr<AudioDestination> AudioDestination::create(AudioIOCallback& callback, const String&, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate)
{
    // FIXME: make use of inputDeviceId as appropriate.

    // FIXME: Add support for local/live audio input.
    if (numberOfInputChannels)
        LOG(Media, "AudioDestination::create(%u, %u, %f) - unhandled input channels", numberOfInputChannels, numberOfOutputChannels, sampleRate);

    // FIXME: Add support for multi-channel (> stereo) output.
    if (numberOfOutputChannels != 2)
        LOG(Media, "AudioDestination::create(%u, %u, %f) - unhandled output channels", numberOfInputChannels, numberOfOutputChannels, sampleRate);

    return std::make_unique<AudioDestinationHaiku>(callback, sampleRate);
}

float AudioDestination::hardwareSampleRate()
{
    media_raw_audio_format format;
    if (BSoundPlayer::GetDefaultMediaFormat(&format) == B_OK)
        return format.frame_rate;
    return 44100;
}

unsigned long AudioDestination::maxChannelCount()
{
    // FIXME: query the default audio hardware device to return the actual number
    // of channels of the device. Also see corresponding FIXME in create().
    return 2;
}


static const unsigned framesToPull = 128; // WebAudio default quantum is 128

AudioDestinationHaiku::AudioDestinationHaiku(AudioIOCallback& callback, float sampleRate)
    : m_callback(callback)
    , m_renderBus(AudioBus::create(2, framesToPull, false))
    , m_soundPlayer(nullptr)
    , m_sampleRate(sampleRate)
    , m_isPlaying(false)
{
    media_raw_audio_format format = {
        m_sampleRate,
        2,
        media_raw_audio_format::B_AUDIO_FLOAT,
        media_multi_audio_format::B_AUDIO_HOST_ENDIAN,
        framesToPull * sizeof(float) * 2
    };

    m_soundPlayer = new BSoundPlayer(&format, "WebAudio", audioCallback, NULL, this);
    if (m_soundPlayer->InitCheck() != B_OK) {
        delete m_soundPlayer;
        m_soundPlayer = nullptr;
    } else {
        m_soundPlayer->SetHasData(true);
    }
}


AudioDestinationHaiku::~AudioDestinationHaiku()
{
    if (m_soundPlayer) {
        m_soundPlayer->Stop();
        delete m_soundPlayer;
    }
}


void AudioDestinationHaiku::start()
{
    if (m_soundPlayer) {
        m_soundPlayer->Start();
        m_isPlaying = true;
    }
}

void AudioDestinationHaiku::stop()
{
    if (m_soundPlayer) {
        m_soundPlayer->Stop();
        m_isPlaying = false;
    }
}

void AudioDestinationHaiku::audioCallback(void* cookie, void* buffer, size_t size, const media_raw_audio_format& format)
{
    AudioDestinationHaiku* destination = static_cast<AudioDestinationHaiku*>(cookie);
    destination->render(buffer, size);
}

void AudioDestinationHaiku::render(void* buffer, size_t size)
{
    size_t numberOfFrames = size / (2 * sizeof(float)); // 2 channels, float

    if (m_renderBus->length() < numberOfFrames)
        m_renderBus = AudioBus::create(2, numberOfFrames, false);

    m_renderBus->setSampleRate(m_sampleRate);

    AudioIOPosition position;
    if (m_soundPlayer)
        position.position = Seconds(m_soundPlayer->CurrentTime() / 1000000.0);
    else
        position.position = 0_s;

    position.timestamp = MonotonicTime::now();

    m_callback.render(*m_renderBus, numberOfFrames, position);

    // Now interleave.
    float* destination = static_cast<float*>(buffer);
    const float* left = m_renderBus->channel(0)->data();
    const float* right = m_renderBus->channel(1)->data();

    // Safety check for lengths
    size_t framesToCopy = std::min(numberOfFrames, m_renderBus->length());

    for (unsigned i = 0; i < framesToCopy; ++i) {
        *destination++ = *left++;
        *destination++ = *right++;
    }

    // Fill remaining if any (should not happen if logic is correct)
    if (framesToCopy < numberOfFrames) {
        memset(destination, 0, (numberOfFrames - framesToCopy) * 2 * sizeof(float));
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
