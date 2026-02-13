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
#include "NotImplemented.h"
#include "wtf/text/CString.h"
#include "wtf/NeverDestroyed.h"

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

    m_mediaLock.Lock();

    cancelLoad();
    delete m_frameBuffer;
    m_mediaLock.Unlock();
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

void MediaPlayerPrivate::didLoad()
{
    // This is now handled inside IdentifyTracks callback to main thread
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
    // TODO should we seek the tracks to 0? reset m_currentTime? other things?
}

void MediaPlayerPrivate::playCallback(void* cookie, void* buffer,
    size_t /*size*/, const media_raw_audio_format& /*format*/)
{
    MediaPlayerPrivate* player = (MediaPlayerPrivate*)cookie;

    if (!player->m_mediaLock.Lock())
        return;

    // Deleting the BMediaFile release the tracks
    if (player->m_audioTrack) {
        // TODO handle the case where there is a video, but no audio track.
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

void MediaPlayerPrivate::play()
{
    if (m_soundPlayer)
        m_soundPlayer->Start();
    m_paused = false;
}

void MediaPlayerPrivate::pause()
{
    if (m_soundPlayer)
        m_soundPlayer->Stop(false);
    m_paused = true;
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
    // TODO handle the case where there is a video, but no audio track.
    if (!m_audioTrack)
        return WTF::MediaTime();
    return WTF::MediaTime::createWithDouble(m_audioTrack->Duration() / 1000000.f);
}

WTF::MediaTime MediaPlayerPrivate::currentTime() const
{
    return WTF::MediaTime::createWithDouble(m_currentTime);
}

void MediaPlayerPrivate::seekToTarget(const SeekTarget& time)
{
    // TODO we should make sure the cache is ready to serve this. The idea is:
    // * Seek the tracks using SeekToTime
    // * The decoder will try to read "somewhere" in the cache. The Read call
    // should block, and check if the data is already downloaded
    // * If not, it should wait for it (and a sufficient buffer)
    //
    // Generally, we shouldn't let the reads to the cache return uninitialized
    // data. Note that we will probably need HTTP range requests support.

    bigtime_t newTime = (bigtime_t)(time.time.toDouble() * 1000000);
    // Usually, seeking the video is rounded to the nearest keyframe. This
    // modifies newTime, and we pass the adjusted value to the audio track, to
    // keep them in sync
    if (m_videoTrack)
        m_videoTrack->SeekToTime(&newTime);
    if (m_audioTrack)
        m_audioTrack->SeekToTime(&newTime);

    m_currentTime = newTime / 1000000.f;
    m_mediaLock.Unlock();
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
    // FIXME: Return actual buffered ranges based on network cache or BMediaFile state.
    // For now, if we have a media file and are playing, assume we have content.
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

            media_format format;
            track->DecodedFormat(&format);

            m_mediaLock.Lock();
            if (format.IsVideo()) {
                if (!m_videoTrack) {
                    m_videoTrack = track;
                    m_frameBuffer = new BBitmap(
                        BRect(0, 0, format.Width() - 1, format.Height() - 1),
                        B_RGB32);
                }
            }

            if (format.IsAudio()) {
                if (!m_audioTrack) {
                    m_audioTrack = track;
                    m_soundPlayer = new BSoundPlayer(&format.u.raw_audio,
                        "HTML5 Audio", playCallback, NULL, this);
                    m_soundPlayer->SetVolume(m_volume);
                    if (!m_paused)
                        m_soundPlayer->Start();
                }
            }
            m_mediaLock.Unlock();
        }
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
