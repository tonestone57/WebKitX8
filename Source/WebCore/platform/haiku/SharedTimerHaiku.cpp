/*
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
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
#include "SharedTimer.h"

#include <Looper.h>
#include <MessageFilter.h>
#include <MessageRunner.h>
#include <support/Locker.h>
#include <support/Autolock.h>
#include <stdio.h>

#define FIRE_MESSAGE 'fire'


namespace WebCore {

class SharedTimerHaiku : public BHandler {
    friend void setSharedTimerFiredFunction(void (*f)());
public:
    static SharedTimerHaiku* instance();

    void start(double);
    void stop();

protected:
    virtual void MessageReceived(BMessage*);

private:
    SharedTimerHaiku();
    ~SharedTimerHaiku();

    void (*m_timerFunction)();
    BMessageRunner* m_runner;
};

SharedTimerHaiku::SharedTimerHaiku()
    : BHandler("WebKit shared timer")
    , m_timerFunction(0)
    , m_runner(0)
{
}

SharedTimerHaiku::~SharedTimerHaiku()
{
    delete m_runner;
}

SharedTimerHaiku* SharedTimerHaiku::instance()
{
    static SharedTimerHaiku* timer;

    if (!timer) {
        BLooper* looper = BLooper::LooperForThread(find_thread(0));
        BAutolock lock(looper);
        timer = new SharedTimerHaiku();
        looper->AddHandler(timer);
    }

    return timer;
}

void SharedTimerHaiku::start(double interval)
{
    stop();
    BMessage msg(FIRE_MESSAGE);
    m_runner = new BMessageRunner(BMessenger(this), &msg, (bigtime_t)(interval * 1000000), 1);
}

void SharedTimerHaiku::stop()
{
    delete m_runner;
    m_runner = nullptr;
}

void SharedTimerHaiku::MessageReceived(BMessage* message)
{
    if (message->what == FIRE_MESSAGE && m_timerFunction)
        m_timerFunction();
    else
        BHandler::MessageReceived(message);
}

// WebCore functions
void setSharedTimerFiredFunction(void (*f)())
{
    SharedTimerHaiku::instance()->m_timerFunction = f;
}

void setSharedTimerFireInterval(double interval)
{
    SharedTimerHaiku::instance()->start(interval);
}

void stopSharedTimer()
{
    SharedTimerHaiku::instance()->stop();
}

void invalidateSharedTimer()
{
}

} // namespace WebCore
