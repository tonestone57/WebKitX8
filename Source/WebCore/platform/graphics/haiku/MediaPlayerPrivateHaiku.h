/*
 * Copyright (C) 2014 Haiku, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaPlayerPrivateHaiku_h
#define MediaPlayerPrivateHaiku_h
#if ENABLE(VIDEO)

#include "MediaPlayerPrivate.h"

#include <wtf/WeakPtr.h>
#include <wtf/Vector.h>
#include <wtf/HashMap.h>

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
#if ENABLE(MEDIA_SOURCE)
class MediaSourcePrivateHaiku;
class SourceBufferPrivateHaiku;
#endif

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
#if ENABLE(MEDIA_SOURCE)
        friend class MediaSourcePrivateHaiku;
#endif

        static void registerMediaEngine(MediaEngineRegistrar);

        MediaPlayerPrivate(MediaPlayer&);
        MediaPlayerPrivate() = delete;

        ~MediaPlayerPrivate();

        void load(const String& url) override;
#if ENABLE(MEDIA_SOURCE)
        void load(const String& url, MediaSourcePrivateClient*) override;
        void addSourceBuffer(SourceBufferPrivateHaiku*);
        void removeSourceBuffer(SourceBufferPrivateHaiku*);
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

#if !RELEASE_LOG_DISABLED
        const Logger& mediaPlayerLogger() { return m_player.mediaPlayerLogger(); }
        const void* mediaPlayerLogIdentifier() { return m_player.mediaPlayerLogIdentifier(); }
#endif

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
        void IdentifyTracks(const String& url);
#if ENABLE(MEDIA_SOURCE)
        void IdentifyTracks(SourceBufferPrivateHaiku* buffer);
#endif
        static int32 videoPlayThread(void* cookie);

        static void playCallback(void*, void*, size_t,
            const media_raw_audio_format&);

        // engine support
        static void getSupportedTypes(WTF::HashSet<WTF::String>&);
        static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);

        mutable bool m_didReceiveData;
        BMediaFile* m_mediaFile;
        BMediaTrack* m_audioTrack;
        BMediaTrack* m_videoTrack;
        BSoundPlayer* m_soundPlayer;
        BBitmap* m_frameBuffer;
        BLocker m_mediaLock;
        Vector<thread_id> m_identifyThreads;
        thread_id m_videoPlayThread;
        mutable PlatformTimeRanges m_buffered;

#if ENABLE(MEDIA_SOURCE)
        RefPtr<MediaSourcePrivateHaiku> m_mediaSourcePrivate;
        // Map source buffer to its associated BMediaFile to manage lifecycle
        HashMap<SourceBufferPrivateHaiku*, BMediaFile*> m_mseMediaFiles;
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
