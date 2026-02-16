/*
 * Copyright (C) 2014 Haiku, Inc.
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

#ifndef MediaPlayerPrivateHaiku_h
#define MediaPlayerPrivateHaiku_h
#if ENABLE(VIDEO)

#include "MediaPlayerPrivate.h"
#include "StreamingDataIO.h"

#include <wtf/WeakPtr.h>

#include <Locker.h>
#include <ObjectList.h>
#include <private/netservices/UrlProtocolAsynchronousListener.h>

class BBitmap;
class BDataIO;
class BMediaFile;
class BMediaTrack;
class BSoundPlayer;
struct media_raw_audio_format;

namespace WebCore {

class MediaPlayerFactoryHaiku;

class MediaPlayerPrivate
    : public MediaPlayerPrivateInterface
    , public CanMakeWeakPtr<MediaPlayerPrivate>
    , public RefCounted<MediaPlayerPrivate>
{
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(MediaPlayerPrivate);
public:
        void ref() const final { RefCounted::ref(); }
        void deref() const final { RefCounted::deref(); }


        friend class MediaPlayerFactoryHaiku;

        static void registerMediaEngine(MediaEngineRegistrar);

        MediaPlayerPrivate(MediaPlayer&);
        MediaPlayerPrivate() = delete;

        ~MediaPlayerPrivate();

        void load(const String& url) override;
#if ENABLE(MEDIA_SOURCE)
        void load(const URL&, const LoadOptions&, MediaSourcePrivateClient&) override;
        void addStreamingSource(RefPtr<StreamingDataController>);
#endif
        void cancelLoad() override;

        void prepareToPlay() override;

        void play() override;
        void pause() override;

        FloatSize naturalSize() const override;
        bool hasAudio() const override;
        bool hasVideo() const override;

        void setPageIsVisible(bool) override;

        WTF::MediaTime duration() const override;
        WTF::MediaTime currentTime() const override;

        void seekToTarget(const SeekTarget&) final;
        bool seeking() const override;
        bool paused() const override;

        void setVolume(float) override;
        void setMuted(bool) override;
        void setRate(double) override;
        void setPreload(MediaPlayer::Preload) override;

        MediaPlayer::NetworkState networkState() const override;
        MediaPlayer::ReadyState readyState() const override;

        WTF::MediaTime maxTimeSeekable() const override { return duration(); }

        PlatformTimeRanges& buffered() const override;
        bool didLoadingProgress() const override;

        uint64_t bytesLoaded() const override;
        uint64_t totalBytes() const override;

        bool acceleratedRendering() const override { return false; }
        MediaPlayer::MovieLoadType movieLoadType() const override;
        String engineDescription() const override { return "Haiku Media Kit"_s; }
        bool platformVolumeConfigurationRequired() const override { return false; }

        void paint(GraphicsContext&, const FloatRect&) override;
        DestinationColorSpace colorSpace() override;

    constexpr MediaPlayerType mediaPlayerType() const final { return MediaPlayerType::Haiku; }
private:
#if ENABLE(MEDIA_SOURCE)
        static void IdentifyTracks(WeakPtr<MediaPlayerPrivate>, const String& url, RefPtr<StreamingDataController> controller = nullptr);
#else
        static void IdentifyTracks(WeakPtr<MediaPlayerPrivate>, const String& url);
#endif
        static int32 videoPlayThread(void* cookie);

        static void playCallback(void*, void*, size_t,
            const media_raw_audio_format&);

        // engine support
        static void getSupportedTypes(WTF::HashSet<WTF::String>&);
        static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);

        mutable bool m_didReceiveData;
        Vector<BMediaFile*> m_mediaFiles;
        BMediaTrack* m_audioTrack;
        BMediaTrack* m_videoTrack;
        BSoundPlayer* m_soundPlayer;
        BBitmap* m_videoBuffer;
        BBitmap* m_drawBuffer;
        BLocker m_mediaLock;
        BLocker m_drawLock;
        Vector<thread_id> m_identifyThreads;
        thread_id m_videoPlayThread;
        mutable PlatformTimeRanges m_buffered;

#if ENABLE(MEDIA_SOURCE)
        Vector<RefPtr<StreamingDataController>> m_pendingControllers;
        Vector<RefPtr<StreamingDataController>> m_activeControllers;
        BLocker m_controllersLock;
#endif

        MediaPlayer& m_player;
        MediaPlayer::NetworkState m_networkState;
        MediaPlayer::ReadyState m_readyState;

        float m_volume;
        double m_rate { 1.0 };
        float m_currentTime;
        bool m_paused;
        bool m_muted { false };
        MediaPlayer::Preload m_preload;
};

}

#endif
#endif
