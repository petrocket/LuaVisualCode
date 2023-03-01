/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#ifndef LUADEBUGGER_COMPONENT_H
#define LUADEBUGGER_COMPONENT_H

#include "LUADebuggerBus.h"
#include <AzFramework/Network/IRemoteTools.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>

#pragma once

namespace dap
{
    class Session;
    class Writer;
}

namespace LUADebugger
{
    class LUADebuggerComponent
        : public AZ::Component
        , public LUADebugger::LUADebuggerRequests::Bus::Handler
        , public AZ::SystemTickBus::Handler
    {
    public:
        AZ_COMPONENT(LUADebuggerComponent, "{DF0E8693-691C-4B79-8B80-F8964C8E63AD}");

        LUADebuggerComponent();
        virtual ~LUADebuggerComponent();

        static void Reflect(AZ::ReflectContext* reflection);

        //////////////////////////////////////////////////////////////////////////
        // AZ::Component
        virtual void Init();
        virtual void Activate();
        virtual void Deactivate();
        //////////////////////////////////////////////////////////////////////////

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        //! AZ::SystemTickBus::Handler overrides.
        //! @{
        void OnSystemTick() override;
        //! @}
        //
        //////////////////////////////////////////////////////////////////////////
        // Enumerate script contexts on the target
        // Request enumeration of available script contexts
        virtual void EnumerateContexts();
        // Request to be attached to script context
        virtual void AttachDebugger(const char* scriptContextName);
        // Request to be detached from current context
        virtual void DetachDebugger();
        // Request enumeration of classes registered in the current context
        virtual void EnumRegisteredClasses(const char* scriptContextName);
        // Request enumeration of eBusses registered in the current context
        virtual void EnumRegisteredEBuses(const char* scriptContextName);
        // Request enumeration of global methods and properties registered in the current context
        virtual void EnumRegisteredGlobals(const char* scriptContextName);
        // create a breakpoint.  The debugName is the name that was given when the script was executed and represents
        // the 'document' (or blob of script) that the breakpoint is for.  The line number is relative to the start of that blob.
        // the combination of line number and debug name uniquely identify a debug breakpoint.
        virtual void CreateBreakpoint(const AZStd::string& debugName, int lineNumber);
        // Remove a previously set breakpoint from the current context
        virtual void RemoveBreakpoint(const AZStd::string& debugName, int lineNumber);
        // Set over current line in current context. Can only be called while context is on a breakpoint.
        virtual void DebugRunStepOver();
        // Set into current line in current context. Can only be called while context is on a breakpoint.
        virtual void DebugRunStepIn();
        // Set out of current line in current context. Can only be called while context is on a breakpoint.
        virtual void DebugRunStepOut();
        // Stop execution in current context. Not supported.
        virtual void DebugRunStop();
        // Continue execution of current context. Can only be called while context is on a breakpoint.
        virtual void DebugRunContinue();
        // Request enumeration of local variables in current context. Can only be called while context is on a breakpoint
        virtual void EnumLocals();
        // Get value of a variable in the current context. Can only be called while context is on a breakpoint
        virtual void GetValue(const AZStd::string& varName);
        // Set value of a variable in the current context. Can only be called while context is on a breakpoint
        // and value should be the structure returned from a previous call to GetValue().
        virtual void SetValue(const AZ::ScriptContextDebug::DebugValue& value);
        // Request current callstack in the current context. Can only be called while context is on a breakpoint
        virtual void GetCallstack();
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // TargetManagerClient
        virtual void DesiredTargetChanged(AZ::u32 newTargetID, AZ::u32 oldTargetID);
        //////////////////////////////////////////////////////////////////////////

     private:
        AzFramework::IRemoteTools* m_remoteTools = nullptr;
        std::unique_ptr<dap::Session> m_dapSession = nullptr;
        std::shared_ptr<dap::Writer> m_dapLog = nullptr;
    };
};

#endif
