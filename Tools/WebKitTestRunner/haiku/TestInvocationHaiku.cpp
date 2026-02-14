/*
 * Copyright (C) 2014 Haiku, inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TestInvocation.h"

#include <Bitmap.h>
#include <BitmapStream.h>
#include <DataIO.h>
#include <TranslatorRoster.h>
#include <wtf/MD5.h>
#include <wtf/Vector.h>
#include <cstdio>
#include <span>

#include "APICast.h"
#include "APIImage.h"
#include "ShareableBitmap.h"
#include "BitmapImage.h"
#include "PlatformWebView.h"
#include "TestController.h"

namespace WTR {

static void printPNG(const unsigned char* data, size_t length, const char* checksum)
{
    printf("Content-Type: image/png\n");
    printf("Content-Length: %lu\n", length);
    if (checksum)
        printf("ActualHash: %s\n", checksum);
    printf("\n");
    fwrite(data, 1, length, stdout);
}

static void computeMD5HashStringForBitmap(BBitmap* bitmap, char hashString[33])
{
    if (!bitmap)
        return;

    BRect bounds = bitmap->Bounds();
    int pixelsWide = bounds.Width() + 1;
    int pixelsHigh = bounds.Height() + 1;
    int bytesPerRow = bitmap->BytesPerRow();
    unsigned char* pixelData = (unsigned char*)bitmap->Bits();

    MD5 md5Context;
    for (int i = 0; i < pixelsHigh; ++i) {
        md5Context.addBytes(std::span<const uint8_t>(pixelData, 4 * pixelsWide));
        pixelData += bytesPerRow;
    }

    MD5::Digest hash;
    md5Context.computeHash(hash);

    snprintf(hashString, 33, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
        hash[0], hash[1], hash[2], hash[3], hash[4], hash[5], hash[6], hash[7],
        hash[8], hash[9], hash[10], hash[11], hash[12], hash[13], hash[14], hash[15]);
}

static void dumpBitmap(BBitmap* bitmap, const char* checksum)
{
    if (!bitmap)
        return;

    BBitmapStream stream(bitmap);

    BMallocIO mio;
    status_t err = BTranslatorRoster::Default()->Translate(&stream, NULL, NULL, &mio, B_PNG_FORMAT);

    BBitmap* out;
    stream.DetachBitmap(&out);

    if (err == B_OK) {
        printPNG((const unsigned char*)mio.Buffer(), mio.BufferLength(), checksum);
    } else {
        fprintf(stderr, "Error translating bitmap: %s\n", strerror(err));
    }
}

void TestInvocation::dumpPixelsAndCompareWithExpected(SnapshotResultType type, WKArrayRef repaintRects, WKImageRef wkImage)
{
    BBitmap* bBitmap = nullptr;
    std::unique_ptr<BBitmap> snapshotBitmap;
    RefPtr<WebCore::BitmapRef> platformImage;

    if (wkImage) {
        auto* apiImage = WebKit::toImpl(wkImage);
        if (apiImage) {
            auto* shareableBitmap = apiImage->resource();
            if (shareableBitmap) {
                platformImage = shareableBitmap->createPlatformImage();
                bBitmap = platformImage.get();
            }
        }
    } else if (type == SnapshotResultType::WebView) {
        bBitmap = TestController::singleton().mainWebView()->windowSnapshotImage();
        // Since windowSnapshotImage returns a pointer we assume ownership of?
        // No, PlatformWebView.h defines PlatformImage as BBitmap*.
        // We usually expect ownership transfer for snapshots.
        // Let's check PlatformWebViewHaiku.cpp implementation later.
    }

    if (!bBitmap)
        return;

    char actualHashMD5[33];
    computeMD5HashStringForBitmap(bBitmap, actualHashMD5);

    if (!compareActualHashToExpectedAndDumpResults(actualHashMD5))
        dumpBitmap(bBitmap, actualHashMD5);

    // If we obtained the bitmap from windowSnapshotImage, we might need to delete it.
    // Ideally TestController/PlatformWebView should manage this or return a smart pointer.
    // For now, assuming if it came from windowSnapshotImage, we own it.
    if (!wkImage && bBitmap) {
        delete bBitmap;
    }
}

}
