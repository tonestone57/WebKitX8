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

    BString content;
    char* buffer = content.LockBuffer(size);
    ssize_t bytesRead = file.Read(buffer, size);
    content.UnlockBuffer(bytesRead > 0 ? bytesRead : 0);

    if (bytesRead < 0)
        return false;

    WTF::String fingerprint = computeSHA256Fingerprint(info);
    BString bHost(host.utf8().data());
    BString bFingerprint(fingerprint.utf8().data());

    int32 start = 0;
    int32 end;
    while ((end = content.FindFirst('\n', start)) != B_ERROR) {
        BString line;
        content.CopyInto(line, start, end - start);
        start = end + 1;

        if (line.IsEmpty()) continue;

        int32 spacePos = line.FindFirst(' ');
        if (spacePos != B_ERROR) {
            BString lineHost;
            line.CopyInto(lineHost, 0, spacePos);
            if (lineHost == bHost) {
                BString lineFingerprint;
                line.CopyInto(lineFingerprint, spacePos + 1, line.Length() - spacePos - 1);
                if (lineFingerprint == bFingerprint)
                    return true;
            }
        } else {
            // Legacy format (host only) - treat as not matching to force upgrade
            if (line == bHost)
                return false;
        }
    }

    // Handle last line if no newline
    if (start < content.Length()) {
        BString line;
        content.CopyInto(line, start, content.Length() - start);
        if (!line.IsEmpty()) {
            int32 spacePos = line.FindFirst(' ');
            if (spacePos != B_ERROR) {
                BString lineHost;
                line.CopyInto(lineHost, 0, spacePos);
                if (lineHost == bHost) {
                    BString lineFingerprint;
                    line.CopyInto(lineFingerprint, spacePos + 1, line.Length() - spacePos - 1);
                    if (lineFingerprint == bFingerprint)
                        return true;
                }
            } else if (line == bHost) {
                return false;
            }
        }
    }

    return false;
}

void addHTTPSCertificateException(const WTF::String& host, const WebCore::CertificateInfo& info)
{
    BPath path = getExceptionFilePath();
    if (path.InitCheck() != B_OK)
        return;

    BString bHost(host.utf8().data());
    BString newContent;

    // Read existing content
    BFile readFile(path.Path(), B_READ_ONLY);
    if (readFile.InitCheck() == B_OK) {
        off_t size;
        readFile.GetSize(&size);
        if (size > 0) {
            BString content;
            char* buffer = content.LockBuffer(size);
            ssize_t bytesRead = readFile.Read(buffer, size);
            content.UnlockBuffer(bytesRead > 0 ? bytesRead : 0);

            if (bytesRead > 0) {
                int32 start = 0;
                int32 end;
                while ((end = content.FindFirst('\n', start)) != B_ERROR) {
                    BString line;
                    content.CopyInto(line, start, end - start);
                    start = end + 1;

                    if (line.IsEmpty()) continue;

                    // Check if this line is for the same host
                    bool isSameHost = false;
                    int32 spacePos = line.FindFirst(' ');
                    if (spacePos != B_ERROR) {
                        BString lineHost;
                        line.CopyInto(lineHost, 0, spacePos);
                        if (lineHost == bHost) isSameHost = true;
                    } else {
                        if (line == bHost) isSameHost = true;
                    }

                    if (!isSameHost) {
                        newContent << line << "\n";
                    }
                }

                // Handle last line
                if (start < content.Length()) {
                    BString line;
                    content.CopyInto(line, start, content.Length() - start);
                    if (!line.IsEmpty()) {
                        bool isSameHost = false;
                        int32 spacePos = line.FindFirst(' ');
                        if (spacePos != B_ERROR) {
                            BString lineHost;
                            line.CopyInto(lineHost, 0, spacePos);
                            if (lineHost == bHost) isSameHost = true;
                        } else {
                            if (line == bHost) isSameHost = true;
                        }

                        if (!isSameHost) {
                            newContent << line << "\n";
                        }
                    }
                }
            }
        }
    }

    WTF::String fingerprint = computeSHA256Fingerprint(info);
    newContent << bHost << " " << fingerprint.utf8().data() << "\n";

    BFile writeFile(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
    if (writeFile.InitCheck() == B_OK) {
        writeFile.Write(newContent.String(), newContent.Length());
    }
}

} // namespace WebKit
