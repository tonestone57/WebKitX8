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
#include "PageUIClientHaiku.h"

#include "WebViewBase.h"
#include <WebCore/FloatSize.h>
#include <PrintJob.h>
#include <Rect.h>
#include <Window.h>
#include <cmath>

namespace WebKit {

PageUIClientHaiku::PageUIClientHaiku(WebViewBase& webView)
    : m_webView(webView)
{
}

PageUIClientHaiku::~PageUIClientHaiku()
{
}

void PageUIClientHaiku::printFrame(WebPageProxy& page, WebFrameProxy& frame, const WebCore::FloatSize& pdfFirstPageSize, CompletionHandler<void()>&& completionHandler)
{
    BPrintJob job("WebKit Print Job");

    if (job.ConfigJob() == B_OK) {
        job.BeginJob();

        BRect printableRect = job.PrintableRect();
        BRect viewRect = m_webView.Bounds();

        // Simple scaling to fit width
        float scale = 1.0f;
        if (viewRect.Width() > printableRect.Width()) {
            scale = printableRect.Width() / viewRect.Width();
        }

        // Calculate number of pages needed for height
        float pageHeightUnscaled = printableRect.Height() / scale;
        int32 pages = static_cast<int32>(ceil(viewRect.Height() / pageHeightUnscaled));
        if (pages < 1) pages = 1;

        for (int32 i = 0; i < pages; i++) {
            BRect pageRect(0, i * pageHeightUnscaled, viewRect.Width(), (i + 1) * pageHeightUnscaled);
            if (pageRect.bottom > viewRect.Height())
                pageRect.bottom = viewRect.Height();

            if (BWindow* window = m_webView.Window()) {
                if (window->Lock()) {
                    // Draw the portion of the view corresponding to the current page
                    job.DrawView(&m_webView, pageRect, printableRect.LeftTop());
                    window->Unlock();
                }
            }
            job.SpoolPage();
        }

        job.CommitJob();
    }

    completionHandler();
}

} // namespace WebKit
