/*
 * Copyright (C) 2016 Adrien Destugues
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MainThreadSharedTimer.h"

#include <Application.h>
#include <Handler.h>
#include <Message.h>
#include <MessageRunner.h>

#include <wtf/Assertions.h>
#include <wtf/MainThread.h>

namespace WebCore {


static BMessageRunner* runner = nullptr;
static uint32_t s_timerGeneration = 0;


class SharedTimerHandlerHaiku: public BHandler
{
public:
    void MessageReceived(BMessage* message) override
    {
        if (message->what == 'shrt')
        {
            uint32_t generation = 0;
            // Legacy check: if no generation is found, it's 0. But new timer starts at 1?
            // Actually let's assume all new messages have generation.
            if (message->FindUInt32("generation", (uint32*)&generation) != B_OK)
                generation = 0;

            // If the generation doesn't match, this is an old message from a stopped timer.
            if (generation != s_timerGeneration)
                return;

            delete runner;
            runner = NULL;
            MainThreadSharedTimer::singleton().fired();
            return;
        }

        BHandler::MessageReceived(message);
    }
};


static SharedTimerHandlerHaiku* handler = nullptr;


void MainThreadSharedTimer::stop()
{
    delete runner;
    runner = NULL;
    // Increment generation to invalidate any pending messages
    s_timerGeneration++;
}

void MainThreadSharedTimer::setFireInterval(WTF::Seconds interval)
{
    if (!handler)
    {
        handler = new SharedTimerHandlerHaiku();
        be_app->AddHandler(handler);
    }

    if (runner) {
        delete runner;
        runner = NULL;
    }

    s_timerGeneration++;
    BMessage msg('shrt');
    msg.AddUInt32("generation", s_timerGeneration);

    runner = new BMessageRunner(handler, &msg, (bigtime_t)interval.microseconds(), 1);
}

void MainThreadSharedTimer::invalidate()
{
}

}
