/*
 * Copyright (C) 2014 Haiku, inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "TestController.h"
#include <Application.h>
#include <OS.h>
#include <stdlib.h>
#include <wtf/Assertions.h>

struct TestRunnerArguments {
    int argc;
    char** argv;
};

static status_t testRunnerThread(void* data)
{
    TestRunnerArguments* args = static_cast<TestRunnerArguments*>(data);

    {
        WTR::TestController controller(args->argc, const_cast<const char**>(args->argv));
        // The controller runs tests in its constructor/destructor lifecycle or explicit run method
        // depending on how it's implemented. Assuming standard WTR behavior:
        // Controller is created, runs, and when destroyed or done, we quit.
    }

    // When controller is done, we quit the app
    if (be_app)
        be_app->PostMessage(B_QUIT_REQUESTED);

    return B_OK;
}

int main(int argc, char** argv)
{
    WTFInstallReportBacktraceOnCrashHook();

    // Create the application on the main thread
    BApplication app("application/x-vnd.haiku-webkit.testrunner");

    // Spawn the test runner thread
    TestRunnerArguments args = { argc, argv };
    thread_id thread = spawn_thread(testRunnerThread, "TestRunnerThread", B_NORMAL_PRIORITY, &args);

    if (thread >= B_OK) {
        resume_thread(thread);
    } else {
        fprintf(stderr, "Failed to spawn test runner thread: %s\n", strerror(thread));
        return 1;
    }

    // Run the application loop
    app.Run();

    return 0;
}
