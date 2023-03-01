/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#include "LUADebugAdapterApplication.h"
#include "LUADebuggerComponent.h"
#include <AzCore/Settings/SettingsRegistryMergeUtils.h>
#include <dap/io.h>
#include <dap/protocol.h>
#include <dap/session.h>
#include <AzCore/Utils/Utils.h>
#include <AzCore/Console/ILogger.h>

namespace LUADebugger
{
    static const char* GetBuildTargetName()
    {
#if !defined(LY_CMAKE_TARGET)
#error "LY_CMAKE_TARGET must be defined in order to add this source file to a CMake executable target"
#endif
        return LY_CMAKE_TARGET;
    }

    LUADebugAdapterApplication::LUADebugAdapterApplication(int* argc, char*** argv)
        : AzFramework::Application(argc, argv)
    {
        // this is a bit too late - Application will already have checked this
        auto settingsRegistry = AZ::SettingsRegistry::Get();
        auto executableDirectory = AZ::Utils::GetExecutableDirectory();
        settingsRegistry->Set("/Amazon/AzCore/Bootstrap/project_path", executableDirectory);

        AZ::SettingsRegistryMergeUtils::MergeSettingsToRegistry_AddBuildSystemTargetSpecialization(
            *AZ::SettingsRegistry::Get(), GetBuildTargetName());

        //AZ_TracePrintf("LUADebugAdapterApplication", "LUADebugAdapterApplication Starting...");
    }


    LUADebugAdapterApplication::~LUADebugAdapterApplication()
    {

    }

    void LUADebugAdapterApplication::StartCommon(AZ::Entity* systemEntity)
    {
        auto settingsRegistry = AZ::SettingsRegistry::Get();
        settingsRegistry->Set("/Amazon/AzCore/Streamer/ReportHardware", false);
        AzFramework::Application::StartCommon(systemEntity);
    }

    void LUADebugAdapterApplication::RunMainLoop()
    {
        // we have to override this to call TickSystem()
        uint32_t frameCounter = 0;
        while (!m_exitMainLoopRequested)
        {
            AzFramework::Application::PumpSystemEventLoopUntilEmpty();
            TickSystem();
            Tick();
            ++frameCounter;
        }
    }

    AZ::ComponentTypeList LUADebugAdapterApplication::GetRequiredSystemComponents() const
    {
        AZ::ComponentTypeList components = AzFramework::Application::GetRequiredSystemComponents();

        components.insert(components.end(), {
            azrtti_typeid<LUADebugger::LUADebuggerComponent>()
            });

        return components;
    }

    void LUADebugAdapterApplication::RegisterCoreComponents()
    {
        AzFramework::Application::RegisterCoreComponents();
        LUADebugger::LUADebuggerComponent::CreateDescriptor();
    }

    void LUADebugAdapterApplication::Reflect(AZ::ReflectContext* context)
    {
        AzFramework::Application::Reflect(context);
        LUADebugger::LUADebuggerComponent::Reflect(context);
    }
}
