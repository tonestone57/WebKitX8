/*
 * Copyright (C) 2024 Haiku, Inc. All rights reserved.
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
#include "CertificateUtilitiesHaiku.h"

#include <WebCore/CertificateInfo.h>
#include <openssl/sha.h>
#include <wtf/HexNumber.h>
#include <wtf/Vector.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

#include <File.h>
#include <FindDirectory.h>
#include <Path.h>
#include <String.h>

namespace WebKit {

static WTF::String computeSHA256Fingerprint(const WebCore::CertificateInfo& info)
{
    if (info.isEmpty() || info.certificateChain().isEmpty())
        return emptyString();

    const auto& certData = info.certificateChain()[0];
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(certData.data(), certData.size(), hash);

    char buffer[SHA256_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        sprintf(&buffer[i * 2], "%02X", hash[i]);

    return WTF::String::fromUTF8(buffer);
}

static BPath getExceptionFilePath()
{
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
        return BPath();

    path.Append("WebKit");
    create_directory(path.Path(), 0755);
    path.Append("certificate_exceptions");
    return path;
}

bool isHTTPSCertificateAllowed(const WTF::String& host, const WebCore::CertificateInfo& info)
{
    BPath path = getExceptionFilePath();
    if (path.InitCheck() != B_OK)
        return false;

    BFile file(path.Path(), B_READ_ONLY);
    if (file.InitCheck() != B_OK)
        return false;

    off_t size;
    file.GetSize(&size);
    if (size <= 0)
        return false;

    WTF::Vector<char> buffer(size + 1);
    if (file.Read(buffer.data(), size) < size)
        return false;
    buffer[size] = '\0';

    WTF::String content = WTF::String::fromUTF8(buffer.data());
    WTF::Vector<WTF::String> lines = content.split('\n');

    WTF::String fingerprint = computeSHA256Fingerprint(info);

    for (const auto& line : lines) {
        WTF::Vector<WTF::String> parts = line.split(' ');
        if (parts.isEmpty())
            continue;

        if (parts[0] == host) {
            // Legacy format: host only
            if (parts.size() == 1)
                return false; // Force upgrade to fingerprint

            // New format: host fingerprint
            if (parts.size() >= 2 && parts[1] == fingerprint)
                return true;
        }
    }

    return false;
}

void addHTTPSCertificateException(const WTF::String& host, const WebCore::CertificateInfo& info)
{
    BPath path = getExceptionFilePath();
    if (path.InitCheck() != B_OK)
        return;

    // Read existing content to filter out old entry for this host
    BFile readFile(path.Path(), B_READ_ONLY);
    WTF::String newContent = emptyString();

    if (readFile.InitCheck() == B_OK) {
        off_t size;
        readFile.GetSize(&size);
        if (size > 0) {
            WTF::Vector<char> buffer(size + 1);
            if (readFile.Read(buffer.data(), size) == size) {
                buffer[size] = '\0';
                WTF::String content = WTF::String::fromUTF8(buffer.data());
                WTF::Vector<WTF::String> lines = content.split('\n');

                for (const auto& line : lines) {
                    if (line.isEmpty()) continue;
                    WTF::Vector<WTF::String> parts = line.split(' ');
                    if (!parts.isEmpty() && parts[0] == host)
                        continue; // Skip existing entry for this host

                    newContent = makeString(newContent, line, "\n"_s);
                }
            }
        }
    }

    WTF::String fingerprint = computeSHA256Fingerprint(info);
    newContent = makeString(newContent, host, " "_s, fingerprint, "\n"_s);

    BFile writeFile(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
    if (writeFile.InitCheck() == B_OK) {
        writeFile.Write(newContent.utf8().data(), newContent.utf8().length());
    }
}

} // namespace WebKit
