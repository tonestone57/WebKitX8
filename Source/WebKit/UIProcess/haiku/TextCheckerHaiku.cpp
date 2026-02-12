/*
 * Copyright (C) 2019 Haiku, Inc.
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
#include "TextChecker.h"

#include "TextCheckerState.h"
#include <wtf/OptionSet.h>

namespace WebKit {
using namespace WebCore;

OptionSet<TextCheckerState> TextChecker::state()
{
    return { };
}

bool TextChecker::isContinuousSpellCheckingAllowed()
{
    return false;
}

bool TextChecker::setContinuousSpellCheckingEnabled(bool)
{
    return false;
}

void TextChecker::setGrammarCheckingEnabled(bool)
{
}

void TextChecker::continuousSpellCheckingEnabledStateChanged(bool)
{
}

void TextChecker::grammarCheckingEnabledStateChanged(bool)
{
}

SpellDocumentTag TextChecker::uniqueSpellDocumentTag(WebPageProxy*)
{
    static SpellDocumentTag tag = 0;
    return ++tag;
}

void TextChecker::closeSpellDocumentWithTag(SpellDocumentTag)
{
}

void TextChecker::checkSpellingOfString(SpellDocumentTag, StringView, int32_t& misspellingLocation, int32_t& misspellingLength)
{
    misspellingLocation = -1;
    misspellingLength = 0;
}

void TextChecker::checkGrammarOfString(SpellDocumentTag, StringView, Vector<WebCore::GrammarDetail>&, int32_t& badGrammarLocation, int32_t& badGrammarLength)
{
    badGrammarLocation = -1;
    badGrammarLength = 0;
}

bool TextChecker::spellingUIIsShowing()
{
    return false;
}

void TextChecker::toggleSpellingUIIsShowing()
{
}

void TextChecker::updateSpellingUIWithMisspelledWord(SpellDocumentTag, const String&)
{
}

void TextChecker::updateSpellingUIWithGrammarString(SpellDocumentTag, const String&, const WebCore::GrammarDetail&)
{
}

void TextChecker::getGuessesForWord(SpellDocumentTag, const String&, const String&, int32_t, Vector<String>&, bool)
{
}

void TextChecker::learnWord(SpellDocumentTag, const String&)
{
}

void TextChecker::ignoreWord(SpellDocumentTag, const String&)
{
}

void TextChecker::requestCheckingOfString(Ref<TextCheckerCompletion>&& completion, int32_t)
{
    completion->didFinishCheckingText({ });
}

void TextChecker::setTestingMode(bool)
{
}

bool TextChecker::isTestingMode()
{
    return false;
}

#if USE(UNIFIED_TEXT_CHECKING)

Vector<TextCheckingResult> TextChecker::checkTextOfParagraph(SpellDocumentTag, StringView, int32_t, OptionSet<TextCheckingType>, bool)
{
    return { };
}

#endif

} // namespace WebKit
