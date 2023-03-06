/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once
#include <AzFramework/Application/Application.h>

namespace LUADebugger
{
    class Platform
    {
    public:
        // TODO implement PAL version
        bool SupportsWaitForDebugger();
        void WaitForDebugger();
        void Printf(const char* format, ...);
    };

    Platform& GetPlatform();

    class LUADebugAdapterApplication final
        : public AzFramework::Application
    {
    public:
        explicit LUADebugAdapterApplication(int* argc, char*** argv);
        ~LUADebugAdapterApplication() override;

        AZ::ComponentTypeList GetRequiredSystemComponents() const override;
        void RegisterCoreComponents() override;

        void RunMainLoop() override;
    protected:
        void Reflect(AZ::ReflectContext* context) override;
        void StartCommon(AZ::Entity* systemEntity) override;
    };
}
