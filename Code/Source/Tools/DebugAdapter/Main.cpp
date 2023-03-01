/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#include "LUADebugAdapterApplication.h"

int main([[maybe_unused]]int argc, [[maybe_unused]]char* argv[])
{
    AZ::Debug::Trace::Instance().SetLogLevel(AZ::Debug::LogLevel::Disabled);
    LUADebugger::LUADebugAdapterApplication app(&argc, &argv);
    app.Start({}, {});
    app.RunMainLoop();
    app.Stop();

    return 0;
}
