/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#include <string>
#include <iostream>
#include <sstream>
#include <memory>
#include "AzCore/Platform.h"
#include "LUADebugAdapterApplication.h"

//! display proper usage of the application
void usage([[maybe_unused]] LUADebugger::Platform& platform)
{
    const int INCORRECT_USAGE = 101;
    const int COULD_NOT_CONNECT_TO_O3DE = 102;
    const int COULD_NOT_CONNECT_TO_VISUALCODE = 103;

    std::stringstream ss;
    ss <<
        "LUAVisualCodeDebugAdapter\n"
        "A LUA debug adapter for VS Code and O3DE.\n"
        "\n"
        "Usage:\n"
        "   LUAVisualCodeDebugAdapter.exe [--wait-for-debugger] [--quiet]\n"
        "\n"
        "Options:\n"
        "   --wait-for-debugger: wait for a debugger to attach to process (on supported platforms)\n"
        "   --verbose: output debug info\n"
        "\n"
        "Exit Codes:\n"
        "   0 - success\n"
        "   1 - failure\n"
        << "   " << INCORRECT_USAGE << " - incorrect usage (see above)\n"
        << "   " << COULD_NOT_CONNECT_TO_O3DE << " - could not connect to an O3DE application to debug\n"
        << "   " << COULD_NOT_CONNECT_TO_VISUALCODE << " - could not connect to Visual Code\n";

    std::cerr << ss.str() << std::endl;
}

int main([[maybe_unused]]int argc, [[maybe_unused]]char* argv[])
{
    LUADebugger::Platform& platform = LUADebugger::GetPlatform();

    bool waitForDebugger = false;
    bool verbose = false;
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "--wait-for-debugger") == 0)
        {
            waitForDebugger = true;
        }
        else if (strcmp(argv[i], "--verbose") == 0)
        {
            verbose = true;
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-?") == 0)
        {
            usage(platform);
            return 0;
        }
    }

    if (waitForDebugger)
    {
        if (platform.SupportsWaitForDebugger())
        {
            std::cout << "Waiting for debugger..." << std::endl;
            platform.WaitForDebugger();
        }
        else
        {
            std::cerr << "Warning - platform does not support --wait-for-debugger feature" << std::endl;
        }
    }
    
    if (!verbose)
    {
        AZ::Debug::Trace::Instance().SetLogLevel(AZ::Debug::LogLevel::Disabled);
    }
    LUADebugger::LUADebugAdapterApplication app(&argc, &argv);
    app.Start({}, {});
    app.RunMainLoop();
    app.Stop();

    return 0;
}
