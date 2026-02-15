/*
 * Copyright (C) 2014-2016 Haiku, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "MediaPlayerPrivateHaiku.h"

#if ENABLE(VIDEO)

#include "GraphicsContext.h"
#include "Logging.h"
#include <cmath>
#include "wtf/text/CString.h"
#include "wtf/NeverDestroyed.h"

#include <support/Autolock.h>
#include <support/Locker.h>
#include <Bitmap.h>
#include <DataIO.h>
#include <HttpRequest.h>
#include <MediaDefs.h>
#include <MediaFile.h>
#include <MediaTrack.h>
#include <SoundPlayer.h>
#include <View.h>

namespace WebCore {

class MediaPlayerFactoryHaiku final : public MediaPlayerFactory {
private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::Haiku; };

    Ref<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer& player) const final
    {
        return adoptRef(* new MediaPlayerPrivate(player));
    }

    void getSupportedTypes(HashSet<String>& types) const final
    {
        return MediaPlayerPrivate::getSupportedTypes(types);
    }

    MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters) const final
    {
        return MediaPlayerPrivate::supportsType(parameters);
    }

    bool supportsKeySystem(const String& keySystem, const String& mimeType) const final
    {
        return false;
        //return MediaPlayerPrivate::supportsKeySystem(keySystem, mimeType);
    }
};

void MediaPlayerPrivate::registerMediaEngine(MediaEngineRegistrar registrar)
{
    registrar(makeUnique<MediaPlayerFactoryHaiku>());
}

MediaPlayerPrivate::MediaPlayerPrivate(MediaPlayer& player)
    : m_didReceiveData(false)
    , m_mediaFile(nullptr)
    , m_audioTrack(nullptr)
    , m_videoTrack(nullptr)
    , m_soundPlayer(nullptr)
    , m_frameBuffer(nullptr)
    , m_identifyThread(-1)
    , m_videoPlayThread(-1)
    , m_player(player)
    , m_networkState(MediaPlayer::NetworkState::Empty)
    , m_readyState(MediaPlayer::ReadyState::HaveNothing)
    , m_volume(1.0)
    , m_currentTime(0.f)
    , m_paused(true)
    , m_muted(false)
    , m_rate(1.0)
    , m_preload(MediaPlayer::Preload::Auto)
{
}

MediaPlayerPrivate::~MediaPlayerPrivate()
{
    delete m_soundPlayer;

    if (m_identifyThread >= 0)
        wait_for_thread(m_identifyThread, NULL);

    if (m_videoPlayThread >= 0)
        wait_for_thread(m_videoPlayThread, NULL);

    BAutolock lock(m_mediaLock);

    cancelLoad();
    delete m_frameBuffer;
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivate::load(const String& url, WebCore::MediaSourcePrivateClient*)
{
    load(url);
}
#endif

void MediaPlayerPrivate::load(const String& url)
{
    // Cleanup from previous request (can this even happen?)
    if (m_soundPlayer)
        m_soundPlayer->Stop(false);
    delete m_soundPlayer;
    m_soundPlayer = nullptr;

    if (m_identifyThread >= 0)
        wait_for_thread(m_identifyThread, NULL);

    if (m_videoPlayThread >= 0)
        wait_for_thread(m_videoPlayThread, NULL);
    m_videoPlayThread = -1;

    m_mediaLock.Lock();
    cancelLoad();
    m_mediaLock.Unlock();

    struct IdentifyParams {
        MediaPlayerPrivate* self;
        String url;
    };
    IdentifyParams* params = new IdentifyParams { this, url };

    m_identifyThread = spawn_thread([](void* data) -> int32 {
        IdentifyParams* params = (IdentifyParams*)data;
        params->self->IdentifyTracks(params->url);
        delete params;
        return 0;
    }, "Media Identify", B_NORMAL_PRIORITY, params);

    resume_thread(m_identifyThread);

    m_networkState = MediaPlayer::NetworkState::Loading;
    m_player.networkStateChanged();
}

void MediaPlayerPrivate::cancelLoad()
{
    // m_mediaLock is expected to be held by caller
    delete m_mediaFile;
    m_mediaFile = nullptr;
    m_audioTrack = nullptr;
    m_videoTrack = nullptr;
}

void MediaPlayerPrivate::prepareToPlay()
{
    // No-op for BMediaFile based playback as we stream/read on demand.
    // Resetting state happens on load().
}

void MediaPlayerPrivate::playCallback(void* cookie, void* buffer,
    size_t /*size*/, const media_raw_audio_format& /*format*/)
{
    MediaPlayerPrivate* player = (MediaPlayerPrivate*)cookie;

    if (!player->m_mediaLock.Lock())
        return;

    // Deleting the BMediaFile release the tracks
    if (player->m_audioTrack) {
        player->m_currentTime = player->m_audioTrack->CurrentTime() / 1000000.f;

        int64 size64;
        if (player->m_audioTrack->ReadFrames(buffer, &size64) != B_OK)
        {
            // Notify that we're done playing...
            player->m_currentTime = player->m_audioTrack->Duration() / 1000000.f;
            player->m_soundPlayer->Stop(false);

            WeakPtr<MediaPlayerPrivate> p = WeakPtr(player);
            callOnMainThread([p] {
                if (!p)
                    return;
                p->m_player.timeChanged();
            });

            player->m_audioTrack = nullptr;
        }
    }

    if (player->m_videoTrack && player->m_audioTrack) {
        if (player->m_videoTrack->CurrentTime()
            < player->m_audioTrack->CurrentTime())
        {
            // Decode a video frame and show it on screen
            int64 count;
            if (player->m_videoTrack->ReadFrames(player->m_frameBuffer->Bits(),
                &count) != B_OK) {
                player->m_videoTrack = nullptr;
            }

            WeakPtr<MediaPlayerPrivate> p = WeakPtr(player);
            callOnMainThread([p] {
                if (!p)
                    return;
                p->m_player.repaint();
            });
        }
    }
    player->m_mediaLock.Unlock();
}

int32 MediaPlayerPrivate::videoPlayThread(void* cookie)
{
    MediaPlayerPrivate* player = (MediaPlayerPrivate*)cookie;
    bigtime_t startTime = system_time() - (bigtime_t)(player->m_currentTime * 1000000.0);

    while (!player->m_paused) {
        bigtime_t now = system_time();
        // Handle seeking or drift: if the expected time vs actual time is too far off, reset base
        bigtime_t currentFrameTime = (bigtime_t)(player->m_currentTime * 1000000.0);
        if (std::abs((now - startTime) - currentFrameTime) > 200000) { // 0.2s tolerance
            startTime = now - currentFrameTime;
        }

        {
            BAutolock lock(player->m_mediaLock);
            if (lock.IsLocked() && player->m_videoTrack) {
                int64 frames = 0;
                media_header header;
                if (player->m_videoTrack->ReadFrames(player->m_frameBuffer->Bits(), &frames, &header) == B_OK) {
                    player->m_currentTime = header.start_time / 1000000.f;

                    WeakPtr<MediaPlayerPrivate> p = WeakPtr(player);
                    callOnMainThread([p] {
                        if (p) {
                            p->m_player.timeChanged();
                            p->m_player.repaint();
                        }
                    });
                } else {
                    // End of stream or error
                    player->m_paused = true;
                }
            }
        }

        // Wait for next frame time
        bigtime_t targetTime = startTime + (bigtime_t)(player->m_currentTime * 1000000.0);
        bigtime_t wait = targetTime - system_time();
        if (wait > 0)
            snooze(wait);
        else
            snooze(1000); // Yield briefly if we are late
    }
    return 0;
}

void MediaPlayerPrivate::play()
{
    m_paused = false;

    if (m_soundPlayer) {
        m_soundPlayer->Start();
    } else if (m_videoTrack && m_videoPlayThread < 0) {
        m_videoPlayThread = spawn_thread(videoPlayThread, "Video Playback", B_NORMAL_PRIORITY, this);
        resume_thread(m_videoPlayThread);
    }
}

void MediaPlayerPrivate::pause()
{
    if (m_soundPlayer)
        m_soundPlayer->Stop(false);

    m_paused = true;
    if (m_videoPlayThread >= 0) {
        wait_for_thread(m_videoPlayThread, NULL);
        m_videoPlayThread = -1;
    }
}

FloatSize MediaPlayerPrivate::naturalSize() const
{
    if (!m_frameBuffer)
        return FloatSize(0,0);

    BRect r(m_frameBuffer->Bounds());
    return FloatSize(r.Width() + 1, r.Height() + 1);
}

bool MediaPlayerPrivate::hasAudio() const
{
    return m_audioTrack;
}

bool MediaPlayerPrivate::hasVideo() const
{
    return m_videoTrack;
}

void MediaPlayerPrivate::setPageIsVisible(bool visible)
{
    if (visible) {
        if (!m_paused)
            play();
    } else {
        if (!m_paused && m_soundPlayer)
            m_soundPlayer->Stop(false);
    }
}

WTF::MediaTime MediaPlayerPrivate::duration() const
{
    if (m_audioTrack)
        return WTF::MediaTime::createWithDouble(m_audioTrack->Duration() / 1000000.f);
    if (m_videoTrack)
        return WTF::MediaTime::createWithDouble(m_videoTrack->Duration() / 1000000.f);
    return WTF::MediaTime();
}

WTF::MediaTime MediaPlayerPrivate::currentTime() const
{
    return WTF::MediaTime::createWithDouble(m_currentTime);
}

void MediaPlayerPrivate::seekToTarget(const SeekTarget& time)
{
    BAutolock lock(m_mediaLock);
    // Seeking logic:
    // BMediaTrack::SeekToTime handles the underlying seek.
    // If the media is streaming, BMediaFile/BMediaTrack handles the buffering or blocking.
    // We update m_currentTime to reflect the seek target immediately.

    bigtime_t newTime = (bigtime_t)(time.time.toDouble() * 1000000);
    // Usually, seeking the video is rounded to the nearest keyframe. This
    // modifies newTime, and we pass the adjusted value to the audio track, to
    // keep them in sync
    if (m_videoTrack)
        m_videoTrack->SeekToTime(&newTime);
    if (m_audioTrack)
        m_audioTrack->SeekToTime(&newTime);

    m_currentTime = newTime / 1000000.f;
}

bool MediaPlayerPrivate::seeking() const
{
    return false;
}

bool MediaPlayerPrivate::paused() const
{
    return m_paused;
}

void MediaPlayerPrivate::setVolume(float volume)
{
    m_volume = volume;
    if (m_soundPlayer)
        m_soundPlayer->SetVolume(m_muted ? 0.0f : m_volume);
}

void MediaPlayerPrivate::setMuted(bool muted)
{
    m_muted = muted;
    if (m_soundPlayer)
        m_soundPlayer->SetVolume(m_muted ? 0.0f : m_volume);
}

void MediaPlayerPrivate::setRate(double rate)
{
    m_rate = rate;
}

void MediaPlayerPrivate::setPreload(MediaPlayer::Preload preload)
{
    m_preload = preload;
}

MediaPlayer::NetworkState MediaPlayerPrivate::networkState() const
{
    return m_networkState;
}

MediaPlayer::ReadyState MediaPlayerPrivate::readyState() const
{
    return m_readyState;
}

PlatformTimeRanges& MediaPlayerPrivate::buffered() const
{
    // BMediaFile handles buffering internally (or blocks). We don't have access to the
    // download progress of the underlying stream if we pass a URL directly.
    // Report the full duration as buffered if we have enough data to start playing,
    // to satisfy WebCore expectations.
    m_buffered.clear();
    if (m_readyState >= MediaPlayer::ReadyState::HaveEnoughData && duration() > MediaTime::zeroTime()) {
        m_buffered.add(MediaTime::zeroTime(), duration());
    }
    return m_buffered;
}

bool MediaPlayerPrivate::didLoadingProgress() const
{
    bool progress = m_didReceiveData;
    m_didReceiveData = false;
    return progress;
}

uint64_t MediaPlayerPrivate::bytesLoaded() const
{
    return 0;
}

uint64_t MediaPlayerPrivate::totalBytes() const
{
    return 0;
}

MediaPlayer::MovieLoadType MediaPlayerPrivate::movieLoadType() const
{
    return MediaPlayer::MovieLoadType::Unknown;
}

void MediaPlayerPrivate::paint(GraphicsContext& context, const FloatRect& r)
{
    if (context.paintingDisabled())
        return;

    if (m_frameBuffer) {
        BView* target = context.platformContext();
        target->SetDrawingMode(B_OP_COPY);
        target->DrawBitmap(m_frameBuffer, r);
    }
}

// #pragma mark - private methods

void MediaPlayerPrivate::IdentifyTracks(const String& url)
{
#if B_HAIKU_VERSION <= B_HAIKU_VERSION_1_BETA_5
    BMediaFile* mediaFile = new BMediaFile(BUrl(url.utf8().data()));
#else
    BMediaFile* mediaFile = new BMediaFile(BUrl(url.utf8().data(), false));
#endif

    status_t err = mediaFile->InitCheck();

    m_mediaLock.Lock();
    m_mediaFile = mediaFile;
    m_mediaLock.Unlock();

    if (err == B_OK) {
        for (int i = m_mediaFile->CountTracks() - 1; i >= 0; i--)
        {
            BMediaTrack* track = m_mediaFile->TrackAt(i);
            if (!track) {
                LOG(Media, "MediaPlayerPrivateHaiku: Failed to get track %d", i);
                continue;
            }

            media_format format;
            memset(&format, 0, sizeof(format));

            if (track->DecodedFormat(&format) != B_OK) {
                 LOG(Media, "MediaPlayerPrivateHaiku: Failed to get decoded format for track %d", i);
                 m_mediaFile->ReleaseTrack(track);
                 continue;
            }

            m_mediaLock.Lock();
            if (format.IsVideo()) {
                if (!m_videoTrack) {
                    // Request B_RGB32 for video to avoid software conversion during blit
                    format.u.raw_video.display.format = B_RGB32;
                    status_t err = track->DecodedFormat(&format);
                    if (err != B_OK) {
                         LOG(Media, "MediaPlayerPrivateHaiku: Failed to set RGB32 format for video track %d, retrying with wildcard", i);
                         format.u.raw_video.display.format = 0; // Wildcard
                         if (track->DecodedFormat(&format) != B_OK) {
                             LOG(Media, "MediaPlayerPrivateHaiku: Failed to get any decoded format for video track %d", i);
                             m_mediaFile->ReleaseTrack(track);
                             m_mediaLock.Unlock();
                             continue;
                         }
                    }

                    m_videoTrack = track;
                    m_frameBuffer = new BBitmap(
                        BRect(0, 0, format.Width() - 1, format.Height() - 1),
                        format.u.raw_video.display.format); // Use the negotiated format
                } else {
                    m_mediaFile->ReleaseTrack(track);
                }
            } else if (format.IsAudio()) {
                if (!m_audioTrack) {
                    m_audioTrack = track;
                    m_soundPlayer = new BSoundPlayer(&format.u.raw_audio,
                        "HTML5 Audio", playCallback, NULL, this);

                    if (m_soundPlayer->InitCheck() != B_OK) {
                        LOG(Media, "MediaPlayerPrivateHaiku: Failed to initialize BSoundPlayer");
                        delete m_soundPlayer;
                        m_soundPlayer = nullptr;
                        m_audioTrack = nullptr;
                        m_mediaFile->ReleaseTrack(track);
                    } else {
                        m_soundPlayer->SetVolume(m_volume);
                        if (!m_paused)
                            m_soundPlayer->Start();
                    }
                } else {
                     m_mediaFile->ReleaseTrack(track);
                }
            } else {
                m_mediaFile->ReleaseTrack(track);
            }
            m_mediaLock.Unlock();
        }
    } else {
        LOG(Media, "MediaPlayerPrivateHaiku: Failed to init BMediaFile: %s", strerror(err));
    }

    // Notify main thread
    WeakPtr<MediaPlayerPrivate> p = WeakPtr(this);
    callOnMainThread([p, err] {
        if (!p) return;
        if (err == B_OK) {
            p->m_player.characteristicChanged();
            p->m_player.durationChanged();
            p->m_player.sizeChanged();
            if (p->m_videoTrack)
                p->m_player.firstVideoFrameAvailable();

            p->m_readyState = MediaPlayer::ReadyState::HaveEnoughData;
            p->m_networkState = MediaPlayer::NetworkState::Loaded;
        } else {
            p->m_readyState = MediaPlayer::ReadyState::HaveMetadata;
            p->m_networkState = MediaPlayer::NetworkState::FormatError;
        }
        p->m_player.networkStateChanged();
        p->m_player.readyStateChanged();
    });

    m_identifyThread = -1;
}

// #pragma mark - static methods

static HashSet<String> mimeTypeCache()
{
    static NeverDestroyed<HashSet<String>> cache;
    static bool typeListInitialized = false;

    if (typeListInitialized)
        return cache;

    int32 cookie = 0;
    media_file_format mfi;

    // Add the types the Haiku Media Kit add-ons advertise support for
    while(get_next_file_format(&cookie, &mfi) == B_OK) {
        cache.get().add(String::fromUTF8(mfi.mime_type));
    }

    typeListInitialized = true;
    return cache;
}

void MediaPlayerPrivate::getSupportedTypes(HashSet<String>& types)
{
    types = mimeTypeCache();
}

MediaPlayer::SupportsType MediaPlayerPrivate::supportsType(const MediaEngineSupportParameters& parameters)
{
    if (parameters.type.isEmpty())
        return MediaPlayer::SupportsType::IsNotSupported;

    // spec says we should not return "probably" if the codecs string is empty
    if (mimeTypeCache().contains(parameters.type.containerType())) {
        return parameters.type.codecs().isEmpty() ? MediaPlayer::SupportsType::MayBeSupported : MediaPlayer::SupportsType::IsSupported;
    }

    return MediaPlayer::SupportsType::IsNotSupported;
}

DestinationColorSpace MediaPlayerPrivate::colorSpace()
{
    return DestinationColorSpace::SRGB();
}

}

#endif
