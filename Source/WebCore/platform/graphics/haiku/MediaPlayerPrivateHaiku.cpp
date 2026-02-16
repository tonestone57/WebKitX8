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
#if ENABLE(MEDIA_SOURCE)
#include "MediaSourcePrivateClient.h"
#endif
#include <algorithm>
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
    , m_audioTrack(nullptr)
    , m_videoTrack(nullptr)
    , m_soundPlayer(nullptr)
    , m_videoBuffer(nullptr)
    , m_drawBuffer(nullptr)
    , m_videoPlayThread(-1)
#if ENABLE(MEDIA_SOURCE)
    , m_controllersLock("MSE Controllers Lock")
#endif
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

#if ENABLE(MEDIA_SOURCE)
    m_controllersLock.Lock();
    for (auto& controller : m_pendingControllers) {
        if (controller)
            controller->setEOS();
    }
    m_pendingControllers.clear();
    for (auto& controller : m_activeControllers) {
        if (controller)
            controller->setEOS();
    }
    m_activeControllers.clear();
    m_controllersLock.Unlock();
#endif

    for (thread_id tid : m_identifyThreads)
        wait_for_thread(tid, NULL);

    if (m_videoPlayThread >= 0)
        wait_for_thread(m_videoPlayThread, NULL);

    BAutolock lock(m_mediaLock);

    cancelLoad();
    delete m_videoBuffer;
    delete m_drawBuffer;
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivate::load(const URL&, const LoadOptions&, MediaSourcePrivateClient&)
{
    // Signal EOS to unblock any pending reads
    m_controllersLock.Lock();
    for (auto& controller : m_pendingControllers) {
        if (controller)
            controller->setEOS();
    }
    m_pendingControllers.clear();
    for (auto& controller : m_activeControllers) {
        if (controller)
            controller->setEOS();
    }
    m_activeControllers.clear();
    m_controllersLock.Unlock();

    // Wait for threads to finish
    for (thread_id tid : m_identifyThreads)
        wait_for_thread(tid, NULL);
    m_identifyThreads.clear();

    if (m_videoPlayThread >= 0)
        wait_for_thread(m_videoPlayThread, NULL);
    m_videoPlayThread = -1;

    // Cleanup resources
    if (m_soundPlayer)
        m_soundPlayer->Stop(false);
    delete m_soundPlayer;
    m_soundPlayer = nullptr;

    m_mediaLock.Lock();
    cancelLoad();
    m_mediaLock.Unlock();

    // In MSE mode, we wait for addStreamingSource calls.
    m_readyState = MediaPlayer::ReadyState::HaveNothing;
    m_networkState = MediaPlayer::NetworkState::Loading;
    m_player.networkStateChanged();
    m_player.readyStateChanged();
}

void MediaPlayerPrivate::addStreamingSource(RefPtr<StreamingDataController> controller)
{
    if (!controller)
        return;

    {
        BAutolock lock(m_controllersLock);
        m_pendingControllers.append(controller);
    }

    struct IdentifyParams {
        MediaPlayerPrivate* self;
        String url;
        RefPtr<StreamingDataController> controller;
    };
    IdentifyParams* params = new IdentifyParams { this, String(), controller };

    thread_id tid = spawn_thread([](void* data) -> int32 {
        IdentifyParams* params = (IdentifyParams*)data;
#if ENABLE(MEDIA_SOURCE)
        params->self->IdentifyTracks(params->url, params->controller);
#else
        params->self->IdentifyTracks(params->url);
#endif
        delete params;
        return 0;
    }, "Media Source Identify", B_NORMAL_PRIORITY, params);

    m_identifyThreads.append(tid);
    resume_thread(tid);
}
#endif

void MediaPlayerPrivate::load(const String& url)
{
#if ENABLE(MEDIA_SOURCE)
    m_controllersLock.Lock();
    for (auto& controller : m_pendingControllers) {
        if (controller)
            controller->setEOS();
    }
    m_pendingControllers.clear();
    for (auto& controller : m_activeControllers) {
        if (controller)
            controller->setEOS();
    }
    m_activeControllers.clear();
    m_controllersLock.Unlock();
#endif

    for (thread_id tid : m_identifyThreads)
        wait_for_thread(tid, NULL);
    m_identifyThreads.clear();

    if (m_videoPlayThread >= 0)
        wait_for_thread(m_videoPlayThread, NULL);
    m_videoPlayThread = -1;

    // Cleanup from previous request (can this even happen?)
    if (m_soundPlayer)
        m_soundPlayer->Stop(false);
    delete m_soundPlayer;
    m_soundPlayer = nullptr;

    m_mediaLock.Lock();
    cancelLoad();
    m_mediaLock.Unlock();

    struct IdentifyParams {
        MediaPlayerPrivate* self;
        String url;
#if ENABLE(MEDIA_SOURCE)
        RefPtr<StreamingDataController> controller;
#endif
    };
    IdentifyParams* params = new IdentifyParams { this, url };

    thread_id tid = spawn_thread([](void* data) -> int32 {
        IdentifyParams* params = (IdentifyParams*)data;
#if ENABLE(MEDIA_SOURCE)
        params->self->IdentifyTracks(params->url, params->controller);
#else
        params->self->IdentifyTracks(params->url);
#endif
        delete params;
        return 0;
    }, "Media Identify", B_NORMAL_PRIORITY, params);

    m_identifyThreads.append(tid);
    resume_thread(tid);

    m_networkState = MediaPlayer::NetworkState::Loading;
    m_player.networkStateChanged();
}

void MediaPlayerPrivate::cancelLoad()
{
    // m_mediaLock is expected to be held by caller
    for (auto* file : m_mediaFiles)
        delete file;
    m_mediaFiles.clear();

#if ENABLE(MEDIA_SOURCE)
    // We already signaled EOS in load()/dtor, now we can clear the list
    // m_activeControllers.clear(); // done in load()/dtor?
    // Wait, cancelLoad is called by load().
    // load() calls cancelLoad() under lock.
    // load() clears the lists under lock.
    // So active controllers are cleared before cancelLoad is called?
    // In load():
    // 1. Lock.
    // 2. Clear pending/active.
    // 3. Unlock.
    // 4. Wait threads.
    // 5. ...
    // 6. Lock.
    // 7. cancelLoad().
    // So active controllers are already cleared.
    // But if cancelLoad is called from other places?
    // It's only called from load() and ~dtor.
    // So we are safe.
#endif

    m_audioTrack = nullptr;
    m_videoTrack = nullptr;

    delete m_videoBuffer;
    m_videoBuffer = nullptr;
    delete m_drawBuffer;
    m_drawBuffer = nullptr;
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
            if (player->m_videoTrack->ReadFrames(player->m_videoBuffer->Bits(),
                &count) != B_OK) {
                player->m_videoTrack = nullptr;
            } else {
                 BAutolock lock(player->m_drawLock);
                 std::swap(player->m_videoBuffer, player->m_drawBuffer);
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
                if (player->m_videoTrack->ReadFrames(player->m_videoBuffer->Bits(), &frames, &header) == B_OK) {
                    player->m_currentTime = header.start_time / 1000000.f;

                    {
                        BAutolock lock(player->m_drawLock);
                        std::swap(player->m_videoBuffer, player->m_drawBuffer);
                    }

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
    if (!m_videoBuffer)
        return FloatSize(0,0);

    BRect r(m_videoBuffer->Bounds());
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

    BAutolock lock(m_drawLock);
    if (m_drawBuffer) {
        BView* target = context.platformContext();
        target->SetDrawingMode(B_OP_COPY);
        target->DrawBitmap(m_drawBuffer, r);
    }
}

// #pragma mark - private methods

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivate::IdentifyTracks(const String& url, RefPtr<StreamingDataController> controller)
#else
void MediaPlayerPrivate::IdentifyTracks(const String& url)
#endif
{
    BMediaFile* mediaFile = nullptr;

    if (!url.isEmpty()) {
#if B_HAIKU_VERSION <= B_HAIKU_VERSION_1_BETA_5
        mediaFile = new BMediaFile(BUrl(url.utf8().data()));
#else
        mediaFile = new BMediaFile(BUrl(url.utf8().data(), false));
#endif
    } else {
#if ENABLE(MEDIA_SOURCE)
        if (controller) {
            m_controllersLock.Lock();
            m_pendingControllers.removeFirst(controller);
            m_activeControllers.append(controller);
            m_controllersLock.Unlock();

            mediaFile = new BMediaFile(new StreamingDataIO(controller.copyRef()));
        }
#endif
    }

    status_t err = mediaFile ? mediaFile->InitCheck() : B_ERROR;

    m_mediaLock.Lock();
    if (mediaFile && err == B_OK)
        m_mediaFiles.append(mediaFile);
    else if (mediaFile)
        delete mediaFile;
    m_mediaLock.Unlock();

    if (err == B_OK) {
        for (int i = mediaFile->CountTracks() - 1; i >= 0; i--)
        {
            BMediaTrack* track = mediaFile->TrackAt(i);
            if (!track) {
                LOG(Media, "MediaPlayerPrivateHaiku: Failed to get track %d", i);
                continue;
            }

            media_format format;
            memset(&format, 0, sizeof(format));

            if (track->DecodedFormat(&format) != B_OK) {
                 LOG(Media, "MediaPlayerPrivateHaiku: Failed to get decoded format for track %d", i);
                 mediaFile->ReleaseTrack(track);
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
                             mediaFile->ReleaseTrack(track);
                             m_mediaLock.Unlock();
                             continue;
                         }
                    }

                    m_videoTrack = track;
                    delete m_videoBuffer;
                    m_videoBuffer = new BBitmap(
                        BRect(0, 0, format.Width() - 1, format.Height() - 1),
                        format.u.raw_video.display.format); // Use the negotiated format
                    {
                        BAutolock lock(m_drawLock);
                        delete m_drawBuffer;
                        m_drawBuffer = new BBitmap(
                            BRect(0, 0, format.Width() - 1, format.Height() - 1),
                            format.u.raw_video.display.format);
                    }
                } else {
                    mediaFile->ReleaseTrack(track);
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
                        mediaFile->ReleaseTrack(track);
                    } else {
                        m_soundPlayer->SetVolume(m_volume);
                        if (!m_paused)
                            m_soundPlayer->Start();
                    }
                } else {
                     mediaFile->ReleaseTrack(track);
                }
            } else {
                mediaFile->ReleaseTrack(track);
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
