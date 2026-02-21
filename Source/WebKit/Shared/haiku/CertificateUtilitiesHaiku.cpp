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

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

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

static BString readAllContent(int fd)
{
    struct stat st;
    if (fstat(fd, &st) != 0)
        return BString();

    if (st.st_size == 0)
        return BString();

    BString content;
    char* buffer = content.LockBuffer(st.st_size);
    ssize_t bytesRead = 0;
    while (bytesRead < st.st_size) {
        ssize_t r = read(fd, buffer + bytesRead, st.st_size - bytesRead);
        if (r < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (r == 0) break;
        bytesRead += r;
    }
    content.UnlockBuffer(bytesRead);
    return content;
}

bool isHTTPSCertificateAllowed(const WTF::String& host, const WebCore::CertificateInfo& info)
{
    BPath path = getExceptionFilePath();
    if (path.InitCheck() != B_OK)
        return false;

    int fd = open(path.Path(), O_RDONLY);
    if (fd < 0)
        return false;

    if (flock(fd, LOCK_SH) != 0) {
        close(fd);
        return false;
    }

    BString content = readAllContent(fd);
    flock(fd, LOCK_UN);
    close(fd);

    if (content.IsEmpty())
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
            }
        }
    }

    return false;
}

bool isHTTPSCertificateAllowed(const WTF::String& host)
{
    BPath path = getExceptionFilePath();
    if (path.InitCheck() != B_OK)
        return false;

    int fd = open(path.Path(), O_RDONLY);
    if (fd < 0)
        return false;

    if (flock(fd, LOCK_SH) != 0) {
        close(fd);
        return false;
    }

    BString content = readAllContent(fd);
    flock(fd, LOCK_UN);
    close(fd);

    if (content.IsEmpty())
        return false;

    BString bHost(host.utf8().data());

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
            if (lineHost == bHost)
                return true;
        } else {
            // Legacy format (host only)
            if (line == bHost)
                return true;
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
                if (lineHost == bHost)
                    return true;
            } else if (line == bHost) {
                return true;
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

    // Open with O_RDWR | O_CREAT to ensure we can read and then write
    int fd = open(path.Path(), O_RDWR | O_CREAT, 0644);
    if (fd < 0)
        return;

    // Exclusive lock for writing
    if (flock(fd, LOCK_EX) != 0) {
        close(fd);
        return;
    }

    BString content = readAllContent(fd);
    BString bHost(host.utf8().data());
    BString newContent;

    // Parse and filter existing content
    if (!content.IsEmpty()) {
        int32 start = 0;
        int32 end;
        while ((end = content.FindFirst('\n', start)) != B_ERROR) {
            BString line;
            content.CopyInto(line, start, end - start);
            start = end + 1;

            if (line.IsEmpty()) continue;

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

    // Append new exception
    WTF::String fingerprint = computeSHA256Fingerprint(info);
    newContent << bHost << " " << fingerprint.utf8().data() << "\n";

    // Rewind and write
    if (ftruncate(fd, 0) == 0 && lseek(fd, 0, SEEK_SET) == 0) {
        ssize_t bytesWritten = 0;
        while (bytesWritten < newContent.Length()) {
            ssize_t w = write(fd, newContent.String() + bytesWritten, newContent.Length() - bytesWritten);
            if (w < 0) {
                if (errno == EINTR) continue;
                break;
            }
            bytesWritten += w;
        }
    }

    flock(fd, LOCK_UN);
    close(fd);
}

} // namespace WebKit
