/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Pasteboard.h"

#include "PasteboardContextHaiku.h"
#include "Color.h"
#include "DocumentFragment.h"
#include "DragData.h"
#include "Editor.h"
#include "Frame.h"
#include "LocalFrameInlines.h"
#include "wtf/URL.h"
#include "SimpleRange.h"
#include "TextResourceDecoder.h"
#include "markup.h"

#include <support/Locker.h>
#include <app/Clipboard.h>
#include <Entry.h>
#include <Message.h>
#include <Path.h>
#include <String.h>
#include <wtf/text/CString.h>

#include <BitmapStream.h>
#include <TranslatorRoster.h>


namespace WebCore {

    std::unique_ptr<Pasteboard> Pasteboard::createForCopyAndPaste(std::unique_ptr<PasteboardContext>&& context)
{
    return std::make_unique<Pasteboard>(std::move(context));
}

#if ENABLE(DRAG_SUPPORT)
std::unique_ptr<Pasteboard> Pasteboard::createForDragAndDrop(std::unique_ptr<PasteboardContext>&& context)
{
    return createForCopyAndPaste(std::move(context));
}

std::unique_ptr<Pasteboard> Pasteboard::create(const DragData& dragData)
{
    return createForCopyAndPaste(dragData.createPasteboardContext());
}
#endif

Pasteboard::Pasteboard(std::unique_ptr<WebCore::PasteboardContext, std::default_delete<WebCore::PasteboardContext> >&&)
{
}


class PasteboardTransaction {
public:
    PasteboardTransaction(const PasteboardContext* context)
    {
        if (auto* hContext = dynamic_cast<const PasteboardContextHaiku*>(context))
            m_message = hContext->message();

        if (!m_message) {
            if (be_clipboard->Lock()) {
                m_clipboardLocked = true;
                m_message = be_clipboard->Data();
            }
        }
    }

    ~PasteboardTransaction()
    {
        if (m_clipboardLocked) {
            if (m_committed)
                be_clipboard->Commit();
            be_clipboard->Unlock();
        }
    }

    BMessage* message() const { return m_message; }
    bool isValid() const { return m_message != nullptr; }

    void commit() { m_committed = true; }
    void clear() { if (m_message) m_message->MakeEmpty(); }

private:
    BMessage* m_message = nullptr;
    bool m_clipboardLocked = false;
    bool m_committed = false;
};

void Pasteboard::writeTrustworthyWebURLsPboardType(const PasteboardURL& url)
{
    write(url);
}

void Pasteboard::writeString(const String& type, const String& data)
{
    PasteboardTransaction transaction(context());
    if (!transaction.isValid())
        return;

    BMessage* bdata = transaction.message();
    bool result = false;

    if (bdata) {
        CString typeUTF8 = type.utf8();
        bdata->RemoveName(typeUTF8.data());

        CString dataUTF8 = data.utf8();
        if (bdata->AddData(typeUTF8.data(), B_MIME_TYPE,
                dataUTF8.data(), dataUTF8.length()) == B_OK)
            result = true;
    }

    if (result)
        transaction.commit();
}

void Pasteboard::writeSelection(const std::optional<SimpleRange>& selectedRange, bool canSmartCopyOrDelete, LocalFrame& frame, ShouldSerializeSelectedTextForDataTransfer)
{
    PasteboardTransaction transaction(context());
    if (!transaction.isValid())
        return;

    transaction.clear();
    BMessage* data = transaction.message();
    if (!data)
        return;

    BString string(frame.editor().selectedText());

    // Replace unwanted representation of blank lines
    const char* utf8BlankLine = "\302\240\n";
    string.ReplaceAll(utf8BlankLine, "\n");

    data->AddData("text/plain", B_MIME_TYPE, string.String(), string.Length());

    BString markupString(serializePreservingVisualAppearance(*selectedRange, nullptr, AnnotateForInterchange::Yes));
    data->AddData("text/html", B_MIME_TYPE, markupString.String(), markupString.Length());

    transaction.commit();
}

void Pasteboard::writePlainText(const String& text, SmartReplaceOption smartReplaceOption)
{
    PasteboardTransaction transaction(context());
    if (!transaction.isValid())
        return;

    transaction.clear();
    BMessage* data = transaction.message();
    if (!data)
        return;

    BString string(text);
    data->AddData("text/plain", B_MIME_TYPE, string.String(), string.Length());
    transaction.commit();
}


void WebCore::Pasteboard::write(WebCore::PasteboardImage const& pasteboardImage)
{
    auto image = pasteboardImage.image;
    if (!image)
        return;

    auto nativeImage = image->nativeImage();
    if (!nativeImage)
        return;

    auto platformImage = nativeImage->platformImage();
    if (!platformImage)
        return;

    PasteboardTransaction transaction(context());
    if (!transaction.isValid())
        return;

    transaction.clear();
    BMessage* data = transaction.message();
    if (!data)
        return;

    // 1. Archive as BBitmap (Haiku internal)
    BMessage archive;
    if (platformImage->Archive(&archive) == B_OK)
        data->AddMessage("image/bitmap", &archive);

    // 2. Export as PNG (Interoperability)
    BTranslatorRoster* roster = BTranslatorRoster::Default();
    if (roster) {
        // BBitmapStream takes the bitmap but we must detach it to prevent deletion
        BBitmapStream stream(platformImage.get());
        BMallocIO outStream;

        // Translate to PNG
        if (roster->Translate(&stream, NULL, NULL, &outStream, B_PNG_FORMAT) == B_OK) {
             data->AddData("image/png", B_MIME_TYPE, outStream.Buffer(), outStream.BufferLength());
        }

        BBitmap* tmp = NULL;
        stream.DetachBitmap(&tmp);
    }

    transaction.commit();
}

void Pasteboard::write(const PasteboardBuffer& buffer)
{
    PasteboardTransaction transaction(context());
    if (!transaction.isValid())
        return;

    transaction.clear();
    BMessage* data = transaction.message();
    if (!data)
        return;

    if (buffer.data) {
        auto contiguous = buffer.data->makeContiguous();
        data->AddData(buffer.type.utf8().data(), B_MIME_TYPE, contiguous->data(), contiguous->size());
    }

    transaction.commit();
}

void WebCore::Pasteboard::write(WebCore::PasteboardWebContent const& content)
{
    PasteboardTransaction transaction(context());
    if (!transaction.isValid())
        return;

    transaction.clear();
    BMessage* data = transaction.message();
    if (!data)
        return;

    data->AddData("text/html", B_MIME_TYPE, content.markup.utf8().data(), content.markup.utf8().length());

    data->AddData("text/plain", B_MIME_TYPE, content.text.utf8().data(), content.text.utf8().length());

    transaction.commit();
}

void WebCore::Pasteboard::writeMarkup(WTF::String const& text)
{
    PasteboardTransaction transaction(context());
    if (!transaction.isValid())
        return;

    transaction.clear();
    BMessage* data = transaction.message();
    if (!data)
        return;

    BString string(text);
    data->AddData("text/html", B_MIME_TYPE, string.String(), string.Length());
    transaction.commit();
}



void Pasteboard::write(const PasteboardURL& url)
{
    ASSERT(!url.url.isEmpty());

    PasteboardTransaction transaction(context());
    if (!transaction.isValid())
        return;

    transaction.clear();

    BMessage* data = transaction.message();
    if (!data)
        return;

    BString string(url.url.string());
    data->AddData("text/plain", B_MIME_TYPE, string.String(), string.Length());

    if (url.url.protocolIs("file"_s)) {
        BEntry entry(url.url.fileSystemPath().utf8().data());
        entry_ref ref;
        if (entry.GetRef(&ref) == B_OK) {
            data->AddRef("refs", &ref);
        }
    }

    transaction.commit();
}

void Pasteboard::write(const Color& color)
{
    PasteboardTransaction transaction(context());
    if (!transaction.isValid())
        return;

    transaction.clear();
    BMessage* data = transaction.message();
    if (!data)
        return;

    auto srgba = color.toColorTypeLossy<SRGBA<uint8_t>>();
    rgb_color rgb = { srgba.red, srgba.green, srgba.blue, srgba.alpha };
    data->AddData("RGBColor", B_RGB_COLOR_TYPE, &rgb, sizeof(rgb_color));

    String hex = color.nameForRenderTheme();
    BString hexStr(hex.utf8().data());
    data->AddData("text/plain", B_MIME_TYPE, hexStr.String(), hexStr.Length());

    transaction.commit();
}

Pasteboard::FileContentState Pasteboard::fileContentState()
{
    PasteboardTransaction transaction(context());
    if (!transaction.isValid())
        return FileContentState::NoFileOrImageData;

    BMessage* data = transaction.message();
    if (!data)
        return FileContentState::NoFileOrImageData;

    if (data->HasRef("refs"))
        return FileContentState::MayContainFilePaths;

    if (data->HasData("image/bitmap", B_MIME_TYPE) || data->HasData("image/png", B_MIME_TYPE) || data->HasData("image/jpeg", B_MIME_TYPE))
        return FileContentState::MayContainImage;

    return FileContentState::NoFileOrImageData;
}

bool Pasteboard::canSmartReplace()
{
    return true;
}


void Pasteboard::read(PasteboardWebContentReader& reader, WebContentReadingPolicy, std::optional<long unsigned int>)
{
    PasteboardTransaction transaction(context());
    if (!transaction.isValid())
        return;

    BMessage* data = transaction.message();
    if (!data)
        return;

    const char* buffer = 0;
    ssize_t bufferLength;

    if (data->FindData("text/html", B_MIME_TYPE, (const void**)&buffer, &bufferLength) == B_OK) {
        String html = String::fromUTF8(std::span<const char>(buffer, bufferLength));
        if (reader.readHTML(html))
            return;
    }

    if (data->FindData("text/plain", B_MIME_TYPE, (const void**)&buffer, &bufferLength) == B_OK) {
        String text = String::fromUTF8(std::span<const char>(buffer, bufferLength));
        if (reader.readPlainText(text))
            return;
    }

    // Also try reading general text if specific MIME types failed but we have something
    if (data->HasData("text/plain", B_MIME_TYPE)) {
         // Already handled above
    } else {
        // Fallback for other types?
    }
}


void Pasteboard::read(PasteboardPlainText& text, WebCore::PlainTextURLReadingPolicy, std::optional<long unsigned int>)
{
    PasteboardTransaction transaction(context());
    if (!transaction.isValid())
        return;

    BMessage* data = transaction.message();
    if (!data)
        return;

    const char* buffer = 0;
    ssize_t bufferLength;
    if (data->FindData("text/plain", B_MIME_TYPE, 
            reinterpret_cast<const void**>(&buffer), &bufferLength) == B_OK)
        text.text = String::fromUTF8(std::span<const char>(buffer, bufferLength));
}

RefPtr<DocumentFragment> Pasteboard::documentFragment(LocalFrame& frame, const SimpleRange& context,
													  bool allowPlainText, bool& chosePlainText)
{
    chosePlainText = false;

    PasteboardTransaction transaction(this->context());
    if (!transaction.isValid())
        return nullptr;

    BMessage* data = transaction.message();
    if (!data)
        return nullptr;

    const char* buffer = 0;
    ssize_t bufferLength;
    if (data->FindData("text/html", B_MIME_TYPE, reinterpret_cast<const void**>(&buffer), &bufferLength) == B_OK) {
        RefPtr<TextResourceDecoder> decoder = TextResourceDecoder::create(ASCIILiteral::fromLiteralUnsafe("text/plain"), PAL::UTF8Encoding(), true);
        StringBuilder html;
        html.append(decoder->decode(std::span<const unsigned char>((const unsigned char*)buffer, bufferLength)));
        html.append(decoder->flush());

        if (!html.isEmpty()) {
            RefPtr<DocumentFragment> fragment = createFragmentFromMarkup(*frame.document(), html.toString(), String(), {});
            if (fragment)
                return fragment;
        }
    }

    if (!allowPlainText)
        return nullptr;

    if (data->FindData("text/plain", B_MIME_TYPE, reinterpret_cast<const void**>(&buffer), &bufferLength) == B_OK) {
        String plainText = String::fromUTF8(std::span<const char>(buffer, bufferLength));

        chosePlainText = true;
        RefPtr<DocumentFragment> fragment = createFragmentFromText(context, plainText);
        if (fragment)
            return fragment;
    }

    return nullptr;
}

bool Pasteboard::hasData()
{
    PasteboardTransaction transaction(context());
    if (!transaction.isValid()) return false;

    BMessage* data = transaction.message();
    if (data)
        return !data->IsEmpty();

    return false;
}

void Pasteboard::clear(const String& type)
{
    PasteboardTransaction transaction(context());
    if (!transaction.isValid()) return;

    BMessage* data = transaction.message();
    if (data) {
        data->RemoveName(BString(type).String());
        transaction.commit();
    }
}

String Pasteboard::readOrigin()
{
    // Haiku clipboard doesn't store origin.
    return String();
}

String Pasteboard::readString(const String& type)
{
    BString result;

    PasteboardTransaction transaction(context());
    if (transaction.isValid()) {
        BMessage* data = transaction.message();
        const char* buffer;
        ssize_t bufferLength;
        if (data) {
            data->FindData(type.utf8().data(), B_MIME_TYPE, 
                reinterpret_cast<const void**>(&buffer), &bufferLength);
        }
        result.SetTo(buffer, bufferLength);
    }

    return String::fromUTF8(result.String());
}

String Pasteboard::readStringInCustomData(const String& type)
{
	return readString(type);
}

void Pasteboard::clear()
{
    PasteboardTransaction transaction(context());
    if (!transaction.isValid()) return;

    transaction.clear();
    transaction.commit();
}

#if ENABLE(DRAG_SUPPORT)
static DragImageRef s_dragImage = nullptr;

void Pasteboard::setDragImage(DragImage image, const IntPoint&)
{
    if (s_dragImage)
        delete s_dragImage;
    // We assume ownership of the bitmap
    s_dragImage = image;
}

// Helper to access the drag image from DragClientHaiku
DragImageRef platformDragImage()
{
    return s_dragImage;
}
#endif


Vector<String> Pasteboard::typesForLegacyUnsafeBindings()
{
    Vector<String> result;

    PasteboardTransaction transaction(context());
    if (transaction.isValid()) {
        BMessage* data = transaction.message();
        if (data) {
            char* name;
            uint32 type;
            int32 count;

            for (int32 i = 0; data->GetInfo(B_ANY_TYPE, i, &name, &type, &count) == B_OK; i++) {
                if (strncmp(name, "be:", 3) == 0)
                    continue;
                result.append(String::fromUTF8(name));
            }
        }
    }

    return result;
}

Vector<String> Pasteboard::typesSafeForBindings(const String&)
{
	return typesForLegacyUnsafeBindings();
}

void Pasteboard::writeCustomData(const WTF::Vector<PasteboardCustomData>& data)
{
	if (!data.isEmpty()) {
		const auto& customData = data[0];
		customData.forEachPlatformString([this] (auto& type, auto& string) {
			writeString(type, string);
		});
	}
}

void Pasteboard::read(WebCore::PasteboardFileReader& reader, std::optional<unsigned long>)
{
    PasteboardTransaction transaction(context());
    if (!transaction.isValid()) return;

    BMessage* data = transaction.message();
    if (!data) return;

    entry_ref ref;
    for (int32 i = 0; data->FindRef("refs", i, &ref) == B_OK; i++) {
        BEntry entry(&ref, true);
        if (entry.InitCheck() == B_OK) {
            BPath path;
            if (entry.GetPath(&path) == B_OK) {
                reader.readFilename(String::fromUTF8(path.Path()));
            }
        }
    }
}


} // namespace WebCore
