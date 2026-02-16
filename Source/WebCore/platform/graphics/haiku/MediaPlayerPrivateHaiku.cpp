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

#if ENABLE(MEDIA_SOURCE)
#include "MediaSourcePrivateHaiku.h"
#include "SourceBufferPrivateHaiku.h"
#endif

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

    cancelLoad(); // Stops threads and aborts IO

    if (m_videoPlayThread >= 0)
        wait_for_thread(m_videoPlayThread, NULL);

    BAutolock lock(m_mediaLock);
    delete m_frameBuffer;
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivate::load(const String& url, WebCore::MediaSourcePrivateClient* client)
{
    // Cleanup previous state
    if (m_soundPlayer)
        m_soundPlayer->Stop(false);
    delete m_soundPlayer;
    m_soundPlayer = nullptr;

    cancelLoad(); // This waits for threads, so we must abort IO inside cancelLoad first

    m_mediaLock.Lock();
    // Reset pointers after cancelLoad
    m_mediaFile = nullptr;
    m_audioTrack = nullptr;
    m_videoTrack = nullptr;
    m_mediaLock.Unlock();

    // Initialize MediaSource
    if (client) {
        m_mediaSourcePrivate = adoptRef(*new MediaSourcePrivateHaiku(*this, *client));
        m_networkState = MediaPlayer::NetworkState::Loading;
        m_readyState = MediaPlayer::ReadyState::HaveNothing;
        m_player.networkStateChanged();
        m_player.readyStateChanged();
    }
}

void MediaPlayerPrivate::addSourceBuffer(SourceBufferPrivateHaiku* buffer)
{
    if (!buffer) return;

    struct IdentifyMSEParams {
        MediaPlayerPrivate* self;
        RefPtr<SourceBufferPrivateHaiku> buffer;
    };

    IdentifyMSEParams* params = new IdentifyMSEParams { this, buffer };

    thread_id id = spawn_thread([](void* data) -> int32 {
        IdentifyMSEParams* params = (IdentifyMSEParams*)data;
        params->self->IdentifyTracks(params->buffer.get());
        delete params;
        return 0;
    }, "MSE Identify", B_NORMAL_PRIORITY, params);

    m_mediaLock.Lock();
    m_identifyThreads.append(id);
    m_mediaLock.Unlock();

    resume_thread(id);
}

void MediaPlayerPrivate::removeSourceBuffer(SourceBufferPrivateHaiku* buffer)
{
    BAutolock lock(m_mediaLock);

    if (m_mseMediaFiles.contains(buffer)) {
        BMediaFile* file = m_mseMediaFiles.get(buffer);

        // Iterate tracks of file to check if they are the active ones and clear them
        for (int32 i = 0; i < file->CountTracks(); i++) {
            BMediaTrack* track = file->TrackAt(i);
            if (track == m_audioTrack) {
                m_audioTrack = nullptr;
            }
            if (track == m_videoTrack) {
                m_videoTrack = nullptr;
            }
        }

        delete file; // This deletes tracks
        m_mseMediaFiles.remove(buffer);
    }
}
#endif

void MediaPlayerPrivate::load(const String& url)
{
    // Cleanup from previous request
    if (m_soundPlayer)
        m_soundPlayer->Stop(false);
    delete m_soundPlayer;
    m_soundPlayer = nullptr;

    cancelLoad();

    m_mediaLock.Lock();
    m_mediaFile = nullptr;
    m_audioTrack = nullptr;
    m_videoTrack = nullptr;
    m_mediaLock.Unlock();

    struct IdentifyParams {
        MediaPlayerPrivate* self;
        String url;
    };
    IdentifyParams* params = new IdentifyParams { this, url };

    thread_id id = spawn_thread([](void* data) -> int32 {
        IdentifyParams* params = (IdentifyParams*)data;
        params->self->IdentifyTracks(params->url);
        delete params;
        return 0;
    }, "Media Identify", B_NORMAL_PRIORITY, params);

    m_mediaLock.Lock();
    m_identifyThreads.append(id);
    m_mediaLock.Unlock();

    resume_thread(id);

    m_networkState = MediaPlayer::NetworkState::Loading;
    m_player.networkStateChanged();
}

void MediaPlayerPrivate::cancelLoad()
{
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSourcePrivate) {
        m_mediaSourcePrivate->abortAllSourceBuffers();
    }
#endif

    // Wait for all identification threads
    Vector<thread_id> threads;
    m_mediaLock.Lock();
    threads = m_identifyThreads;
    m_identifyThreads.clear();
    m_mediaLock.Unlock();

    for (auto id : threads) {
        wait_for_thread(id, NULL);
    }

    if (m_videoPlayThread >= 0) {
        m_paused = true;
        wait_for_thread(m_videoPlayThread, NULL);
        m_videoPlayThread = -1;
    }

    BAutolock lock(m_mediaLock);
    delete m_mediaFile;
    m_mediaFile = nullptr;
    m_audioTrack = nullptr;
    m_videoTrack = nullptr;

#if ENABLE(MEDIA_SOURCE)
    m_mediaSourcePrivate = nullptr;
    // Delete all BMediaFiles created for MSE
    for (auto& file : m_mseMediaFiles.values()) {
        delete file;
    }
    m_mseMediaFiles.clear();
#endif
}

void MediaPlayerPrivate::prepareToPlay()
{
}

void MediaPlayerPrivate::playCallback(void* cookie, void* buffer,
    size_t /*size*/, const media_raw_audio_format& /*format*/)
{
    MediaPlayerPrivate* player = (MediaPlayerPrivate*)cookie;

    if (!player->m_mediaLock.Lock())
        return;

    if (player->m_audioTrack) {
        player->m_currentTime = player->m_audioTrack->CurrentTime() / 1000000.f;

        int64 size64;
        if (player->m_audioTrack->ReadFrames(buffer, &size64) != B_OK)
        {
            player->m_currentTime = player->m_audioTrack->Duration() / 1000000.f;
            player->m_soundPlayer->Stop(false);

            WeakPtr<MediaPlayerPrivate> p = WeakPtr(player);
            callOnMainThread([p] {
                if (!p) return;
                p->m_player.timeChanged();
            });
        }
    }

    if (player->m_videoTrack && player->m_audioTrack) {
        if (player->m_videoTrack->CurrentTime() < player->m_audioTrack->CurrentTime()) {
            int64 count;
            if (player->m_videoTrack->ReadFrames(player->m_frameBuffer->Bits(), &count) != B_OK) {
            }

            WeakPtr<MediaPlayerPrivate> p = WeakPtr(player);
            callOnMainThread([p] {
                if (!p) return;
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
        bigtime_t currentFrameTime = (bigtime_t)(player->m_currentTime * 1000000.0);
        if (std::abs((now - startTime) - currentFrameTime) > 200000) {
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
                    player->m_paused = true;
                }
            }
        }

        bigtime_t targetTime = startTime + (bigtime_t)(player->m_currentTime * 1000000.0);
        bigtime_t wait = targetTime - system_time();
        if (wait > 0)
            snooze(wait);
        else
            snooze(1000);
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
    bigtime_t newTime = (bigtime_t)(time.time.toDouble() * 1000000);
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

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivate::IdentifyTracks(SourceBufferPrivateHaiku* buffer)
{
    BDataIO* io = buffer->dataIO();
    BMediaFile* mediaFile = new BMediaFile(io, B_MEDIA_FILE_NO_CLOSING);

    status_t err = mediaFile->InitCheck();

    m_mediaLock.Lock();
    if (err == B_OK) {
        m_mseMediaFiles.add(buffer, mediaFile);
    } else {
        delete mediaFile;
        mediaFile = nullptr;
    }
    m_mediaLock.Unlock();

    if (err != B_OK) {
        LOG(Media, "MediaPlayerPrivateHaiku: Failed to init BMediaFile from SourceBuffer: %s", strerror(err));
        return;
    }

    m_mediaLock.Lock();
    for (int i = mediaFile->CountTracks() - 1; i >= 0; i--)
    {
        BMediaTrack* track = mediaFile->TrackAt(i);
        if (!track) continue;

        media_format format;
        memset(&format, 0, sizeof(format));
        if (track->DecodedFormat(&format) != B_OK) {
             mediaFile->ReleaseTrack(track);
             continue;
        }

        if (format.IsVideo()) {
            if (!m_videoTrack) {
                format.u.raw_video.display.format = B_RGB32;
                if (track->DecodedFormat(&format) != B_OK) {
                     format.u.raw_video.display.format = 0;
                     if (track->DecodedFormat(&format) != B_OK) {
                         mediaFile->ReleaseTrack(track);
                         continue;
                     }
                }
                m_videoTrack = track;
                m_frameBuffer = new BBitmap(
                    BRect(0, 0, format.Width() - 1, format.Height() - 1),
                    format.u.raw_video.display.format);
            } else {
                mediaFile->ReleaseTrack(track);
            }
        } else if (format.IsAudio()) {
            if (!m_audioTrack) {
                m_audioTrack = track;
                m_soundPlayer = new BSoundPlayer(&format.u.raw_audio,
                    "HTML5 Audio", playCallback, NULL, this);

                if (m_soundPlayer->InitCheck() != B_OK) {
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
    }
    m_mediaLock.Unlock();

    WeakPtr<MediaPlayerPrivate> p = WeakPtr(this);
    callOnMainThread([p] {
        if (!p) return;
        p->m_player.characteristicChanged();
        p->m_player.durationChanged();
        p->m_player.sizeChanged();
        if (p->m_videoTrack)
            p->m_player.firstVideoFrameAvailable();

        p->m_readyState = MediaPlayer::ReadyState::HaveEnoughData;
        p->m_networkState = MediaPlayer::NetworkState::Loaded;
        p->m_player.networkStateChanged();
        p->m_player.readyStateChanged();
    });
}
#endif

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
            if (!track) continue;

            media_format format;
            memset(&format, 0, sizeof(format));

            if (track->DecodedFormat(&format) != B_OK) {
                 m_mediaFile->ReleaseTrack(track);
                 continue;
            }

            m_mediaLock.Lock();
            if (format.IsVideo()) {
                if (!m_videoTrack) {
                    format.u.raw_video.display.format = B_RGB32;
                    status_t err = track->DecodedFormat(&format);
                    if (err != B_OK) {
                         format.u.raw_video.display.format = 0;
                         if (track->DecodedFormat(&format) != B_OK) {
                             m_mediaFile->ReleaseTrack(track);
                             m_mediaLock.Unlock();
                             continue;
                         }
                    }

                    m_videoTrack = track;
                    m_frameBuffer = new BBitmap(
                        BRect(0, 0, format.Width() - 1, format.Height() - 1),
                        format.u.raw_video.display.format);
                } else {
                    m_mediaFile->ReleaseTrack(track);
                }
            } else if (format.IsAudio()) {
                if (!m_audioTrack) {
                    m_audioTrack = track;
                    m_soundPlayer = new BSoundPlayer(&format.u.raw_audio,
                        "HTML5 Audio", playCallback, NULL, this);

                    if (m_soundPlayer->InitCheck() != B_OK) {
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

static HashSet<String> mimeTypeCache()
{
    static NeverDestroyed<HashSet<String>> cache;
    static bool typeListInitialized = false;

    if (typeListInitialized)
        return cache;

    int32 cookie = 0;
    media_file_format mfi;

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
