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
#include <DataIO.h>

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

    // Accumulate data using SharedBufferBuilder
    // FIXME: In the future, parse this data instead of just accumulating it.
    // For now, if we don't consume it, it's a memory leak if we keep appending.
    // Since this backend is currently experimental/stubbed, we accumulate for
    // demonstration of the architecture, but we should be careful.

    // m_builder.append(data.get());

    // FIXME: Re-enable buffering when we actually consume the data.
    // For now, dropping data to prevent leaks in the stub.

    return MediaPromise::createAndResolve();
}

void SourceBufferPrivateHaiku::resetParserStateInternal()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_builder = SharedBufferBuilder();
}

void SourceBufferPrivateHaiku::removedFromMediaSource()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_builder = SharedBufferBuilder();
    SourceBufferPrivate::removedFromMediaSource();
}

Ref<SharedBuffer> SourceBufferPrivateHaiku::copyData() const
{
    return m_builder.copyBuffer()->makeContiguous();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)
