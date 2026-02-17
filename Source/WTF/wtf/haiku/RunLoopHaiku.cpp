/*
 * Copyright (C) 2014, 2024 Haiku, Inc
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
#include <wtf/RunLoop.h>

#include <Application.h>
#include <Handler.h>
#include <Looper.h>
#include <MessageQueue.h>
#include <MessageRunner.h>
#include <OS.h>
#include <stdio.h>
#include <errno.h>

#include <wtf/HashSet.h>

namespace WTF {

class LoopHandler: public BHandler
{
    public:
        LoopHandler(RunLoop& runLoop)
            : BHandler("RunLoop")
            , m_runLoop(runLoop)
        {
        }

        ~LoopHandler()
        {
            if (m_runLoop.m_handler == this)
                m_runLoop.m_handler = nullptr;
        }

        void registerTimer(RunLoop::TimerBase* timer)
        {
            m_activeTimers.add(timer);
        }

        void unregisterTimer(RunLoop::TimerBase* timer)
        {
            m_activeTimers.remove(timer);
        }

        void MessageReceived(BMessage* message) override
        {
            if (message->what == 'loop') {
                RunLoop::currentSingleton().performWork();
            } else if (message->what == 'tmrf') {
                RunLoop::TimerBase* timer
                    = (RunLoop::TimerBase*)message->GetPointer("timer");
                if (timer && m_activeTimers.contains(timer))
                    timer->timerFired();
            } else {
                BHandler::MessageReceived(message);
            }
        }

    private:
        RunLoop& m_runLoop;
        HashSet<RunLoop::TimerBase*> m_activeTimers;
};

RunLoop::RunLoop()
    : m_looper(nullptr)
    , m_handler(new LoopHandler(*this))
{
    // Find the looper that we should attach our handler to.
    BLooper* looper;
    BLooper* currentLooper = BLooper::LooperForThread(find_thread(NULL));
    if (currentLooper) {
        // This thread already has a looper (likely the BApplication looper).
        // Attach our handler to it.
        looper = currentLooper;
    } else {
        thread_info main_thread;
        int32 cookie = 0;
        get_next_thread_info(0, &cookie, &main_thread);
        if (find_thread(NULL) == main_thread.thread) {
            if (be_app)
                looper = be_app;
            else {
                // Fallback: create a new Looper for the main thread if be_app is missing.
                m_looper = looper = new BLooper("MainRunLoop");
            }
        } else {
            // No existing BLooper or BApplication is on this thread. Let's
            // create one and manage its lifecycle.
            m_looper = looper = new BLooper("RunLoop");
        }
    }

    if (looper->IsLocked()) {
        looper->AddHandler(m_handler);
    } else {
        looper->Lock();
        looper->AddHandler(m_handler);
        looper->Unlock();
    }
}

RunLoop::~RunLoop()
{
    stop();
    if (m_handler)
        delete m_handler;
}

void RunLoop::run()
{
    // There might already be messages available to process, so lets address
    // those if there are any.
    currentSingleton().wakeUp();

    if (currentSingleton().m_looper) {
        // We created this looper, so we are responsible for running it.
        // BLooper::Loop() blocks until the looper is quit.
        currentSingleton().m_looper->Loop();
    } else if (be_app && find_thread(NULL) == be_app->Thread()) {
        // If we are attached to BApplication, ensuring it runs is necessary for blocking behavior
        // expected by WebKit main function.
        // BApplication::Run() blocks until Quit is called.
        // We only call this if we are on the main thread and be_app exists.
        // If be_app is already running (e.g. nested call), calling Run() again is an error on Haiku.
        // However, standard BApplication usage implies Run() is called once.
        // Since we don't know if it's running, we assume we need to start it if we are asked to run().
        // If it is already running, this might throw/debugger, but in that case we shouldn't be here
        // unless called from within the loop (which is rare for RunLoop::run()).
        be_app->Run();
    }
}

void RunLoop::stop()
{
    if (!m_handler)
        return;

    if (!m_handler->LockLooper())
        return;

    BLooper* looper = m_handler->Looper();
    looper->RemoveHandler(m_handler);
    looper->Unlock();

    if (m_looper) {
        m_looper->PostMessage(B_QUIT_REQUESTED);
        m_looper = nullptr;
    }
}

void RunLoop::wakeUp()
{
    if (m_handler && m_handler->Looper())
        m_handler->Looper()->PostMessage('loop', m_handler);
}

// TimerBase implementation

RunLoop::TimerBase::TimerBase(Ref<RunLoop>&& runLoop, ASCIILiteral description)
    : m_runLoop(WTF::move(runLoop))
    , m_description(description)
    , m_messageRunner(nullptr)
    , m_isRepeating(false)
    , m_interval(0_s)
{
}

RunLoop::TimerBase::~TimerBase()
{
    stop();
}

void RunLoop::TimerBase::timerFired()
{
    if (!m_messageRunner)
        return;

    fired();

    if (m_isRepeating) {
        m_nextFireDate += m_interval;
    } else {
        stop();
    }
}

void RunLoop::TimerBase::start(Seconds nextFireInterval, bool repeat)
{
    stop();
    m_isRepeating = repeat;
    m_interval = nextFireInterval;
    m_nextFireDate = MonotonicTime::now() + m_interval;

    BMessage* message = new BMessage('tmrf');
    message->AddPointer("timer", this);

    bigtime_t interval = (bigtime_t)nextFireInterval.microseconds();

    if (m_runLoop->m_handler) {
        LoopHandler* handler = static_cast<LoopHandler*>(m_runLoop->m_handler);
        handler->registerTimer(this);

        m_messageRunner = new BMessageRunner(m_runLoop->m_handler,
            message, interval, repeat ? -1 : 1);

        if (m_messageRunner->InitCheck() != B_OK) {
            delete m_messageRunner;
            m_messageRunner = nullptr;
            handler->unregisterTimer(this);
        }
    }
    delete message;
}

bool RunLoop::TimerBase::isActive() const
{
    return m_messageRunner != nullptr;
}

void RunLoop::TimerBase::stop()
{
    if (m_messageRunner) {
        delete m_messageRunner;
        m_messageRunner = nullptr;

        if (m_runLoop->m_handler) {
            LoopHandler* handler = static_cast<LoopHandler*>(m_runLoop->m_handler);
            handler->unregisterTimer(this);
        }
    }
}

Seconds RunLoop::TimerBase::secondsUntilFire() const
{
    if (isActive())
        return std::max(0_s, m_nextFireDate - MonotonicTime::now());
    return 0_s;
}

RunLoop::CycleResult RunLoop::cycle(RunLoopMode)
{
    RunLoop::currentSingleton().performWork();

    if (RunLoop::currentSingleton().m_handler) {
        BLooper* looper = RunLoop::currentSingleton().m_handler->Looper();
        if (looper) {
            BMessageQueue* queue = looper->MessageQueue();
            if (queue && !queue->IsEmpty())
                return CycleResult::Continue;
        }
    }

    return CycleResult::Stop;
}

} // namespace WTF
