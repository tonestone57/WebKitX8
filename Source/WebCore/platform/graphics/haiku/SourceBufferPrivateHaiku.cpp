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

#include "config.h"
#include "SourceBufferPrivateHaiku.h"

#if ENABLE(MEDIA_SOURCE)

#include "MediaSourcePrivateHaiku.h"
#include <wtf/Logger.h>

namespace WebCore {

Ref<SourceBufferPrivateHaiku> SourceBufferPrivateHaiku::create(MediaSourcePrivateHaiku& mediaSource, const ContentType& contentType)
{
    return adoptRef(*new SourceBufferPrivateHaiku(mediaSource, contentType));
}

SourceBufferPrivateHaiku::SourceBufferPrivateHaiku(MediaSourcePrivateHaiku& mediaSource, const ContentType& contentType)
    : SourceBufferPrivate(mediaSource)
    , m_contentType(contentType)
#if !RELEASE_LOG_DISABLED
    , m_logger(mediaSource.logger())
    , m_logIdentifier(Logger::nextLogIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);
}

SourceBufferPrivateHaiku::~SourceBufferPrivateHaiku()
{
    ALWAYS_LOG(LOGIDENTIFIER);
}

Ref<MediaPromise> SourceBufferPrivateHaiku::appendInternal(Ref<SharedBuffer>&& data)
{
    ALWAYS_LOG(LOGIDENTIFIER, "data size: ", data->size());
    // FIXME: Implement parsing and append logic
    // For now, just resolve the promise to simulate success
    return MediaPromise::createAndResolve();
}

void SourceBufferPrivateHaiku::resetParserStateInternal()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    // FIXME: Implement
}

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)
