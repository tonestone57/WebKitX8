/*
 * Copyright (C) 2017 Haiku, inc
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

#pragma once

#include "KeyedCoding.h"

#include <Message.h>

#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class KeyedDecoderHaiku final : public KeyedDecoder {
public:
    KeyedDecoderHaiku(std::span<const uint8_t> data);
    ~KeyedDecoderHaiku() override;

private:
    bool decodeBytes(const String& key, std::span<const uint8_t>&) override;
    bool decodeBool(const String& key, bool&) override;
    bool decodeUInt32(const String& key, uint32_t&) override;
    bool decodeInt32(const String& key, int32_t&) override;
    bool decodeUInt64(const String& key, uint64_t&) override;
    bool decodeInt64(const String& key, int64_t&) override;
    bool decodeFloat(const String& key, float&) override;
    bool decodeDouble(const String& key, double&) override;
    bool decodeString(const String& key, String&) override;

    bool beginObject(const String& key) override;
    void endObject() override;

    bool beginArray(const String& key) override;
    bool beginArrayElement() override;
    void endArrayElement() override;
    void endArray() override;

    Vector<BMessage*, 16> m_messageStack;
    Vector<std::pair<String, int>, 16> m_keyStack;
    BMessage* currentMessage;
};

} // namespace WebCore
