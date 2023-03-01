/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "LUADebuggerComponent.h"

#include <AzCore/Interface/Interface.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/PlatformIncl.h>
#include <AzCore/Math/Crc.h>

#include <AzCore/std/parallel/mutex.h>
#include <AzCore/std/parallel/lock.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Script/ScriptContext.h>

#include <AzFramework/Script/ScriptDebugMsgReflection.h>
#include <AzFramework/Script/ScriptRemoteDebuggingConstants.h>

#include <AzToolsFramework/API/EditorAssetSystemAPI.h>
#include <AzCore/Utils/Utils.h>

#include "dap/io.h"
#include "dap/protocol.h"
#include "dap/session.h"

#define LOG_TO_FILE "dap.log"


namespace LUADebugger
{
    // Utility functions
    // Returns true if a valid target was found, in which case the info is returned in targetInfo.
    bool GetDesiredTarget(AzFramework::RemoteToolsEndpointInfo& targetInfo)
    {
        // discover what target the user is currently connected to, if any?

        AzFramework::IRemoteTools* remoteTools = AzFramework::RemoteToolsInterface::Get();
        if (!remoteTools)
        {
            return false;
        }

        AzFramework::RemoteToolsEndpointInfo info = remoteTools->GetDesiredEndpoint(AzFramework::LuaToolsKey);
        if (!info.GetPersistentId())
        {
            AZ_TracePrintf("Debug", "The user has not chosen a target to connect to.\n");
            return false;
        }

        targetInfo = info;
        if (!targetInfo.IsValid() || !targetInfo.IsOnline())
        {
            AZ_TracePrintf("Debug", "The target is currently not in a state that would allow debugging code (offline or not debuggable)\n");
            return false;
        }

        return true;
    }

    LUADebuggerComponent::LUADebuggerComponent()
    {
        Sleep(10*1000);
        m_dapSession = dap::Session::create();

        const dap::integer threadId = 100;
        const dap::integer frameId = 200;
        const dap::integer variablesReferenceId = 300;
        const dap::integer sourceReferenceId = 400;

#ifdef LOG_TO_FILE
        auto executableDirectory = AZ::Utils::GetExecutableDirectory();
        AZ::IO::FixedMaxPath path{executableDirectory};
        path /= LOG_TO_FILE;
        m_dapLog = dap::file(path.c_str());
#endif

        m_dapSession->onError([&](const char* msg) {
            if (m_dapLog) {
                dap::writef(m_dapLog, "\ndap::Session error: %s\n", msg);
                m_dapLog->close();
            }
            });

        // The Initialize request is the first message sent from the client and
        // the response reports debugger capabilities.
        // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Initialize
        m_dapSession->registerHandler([](const dap::InitializeRequest&) {
            dap::InitializeResponse response;
            response.supportsConfigurationDoneRequest = true;
            return response;
            });

        // When the Initialize response has been sent, we need to send the initialized
        // event.
        // We use the registerSentHandler() to ensure the event is sent *after* the
        // initialize response.
        // https://microsoft.github.io/debug-adapter-protocol/specification#Events_Initialized
        m_dapSession->registerSentHandler(
            [&](const dap::ResponseOrError<dap::InitializeResponse>&) {
                m_dapSession->send(dap::InitializedEvent());
                if (m_dapLog)
                {
                    dap::writef(m_dapLog, "\ndap::Session initialized\n");
                }
            });

        // The Threads request queries the debugger's list of active threads.
        // This example debugger only exposes a single thread.
        // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Threads
        m_dapSession->registerHandler([&](const dap::ThreadsRequest&) {
            dap::ThreadsResponse response;
            dap::Thread thread;
            thread.id = threadId;
            thread.name = "MainThread";
            response.threads.push_back(thread);
            return response;
            });

        // The StackTrace request reports the stack frames (call stack) for a given
        // thread. This example debugger only exposes a single stack frame for the
        // single thread.
        // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_StackTrace
        m_dapSession->registerHandler(
            [&](const dap::StackTraceRequest& request)
            -> dap::ResponseOrError<dap::StackTraceResponse> {
                if (request.threadId != threadId) {
                    return dap::Error("Unknown threadId '%d'", int(request.threadId));
                }

                dap::Source source;
                source.sourceReference = sourceReferenceId;
                source.name = "HelloDebuggerSource";

                dap::StackFrame frame;
                frame.line = 0; // debugger.currentLine();
                frame.column = 1;
                frame.name = "HelloDebugger";
                frame.id = frameId;
                frame.source = source;

                dap::StackTraceResponse response;
                response.stackFrames.push_back(frame);
                return response;
            });

        // The Scopes request reports all the scopes of the given stack frame.
        // This example debugger only exposes a single 'Locals' scope for the single
        // frame.
        // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Scopes
        m_dapSession->registerHandler([&](const dap::ScopesRequest& request)
            -> dap::ResponseOrError<dap::ScopesResponse> {
                if (request.frameId != frameId) {
                    return dap::Error("Unknown frameId '%d'", int(request.frameId));
                }

                dap::Scope scope;
                scope.name = "Locals";
                scope.presentationHint = "locals";
                scope.variablesReference = variablesReferenceId;

                dap::ScopesResponse response;
                response.scopes.push_back(scope);
                return response;
            });

        // The Variables request reports all the variables for the given scope.
        // This example debugger only exposes a single 'currentLine' variable for the
        // single 'Locals' scope.
        // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Variables
        m_dapSession->registerHandler([&](const dap::VariablesRequest& request)
            -> dap::ResponseOrError<dap::VariablesResponse> {
                if (request.variablesReference != variablesReferenceId) {
                    return dap::Error("Unknown variablesReference '%d'",
                        int(request.variablesReference));
                }

                dap::Variable currentLineVar;
                currentLineVar.name = "currentLine";
                currentLineVar.value = "0"; // std::to_string(debugger.currentLine());
                currentLineVar.type = "int";

                dap::VariablesResponse response;
                response.variables.push_back(currentLineVar);
                return response;
            });


        // The Pause request instructs the debugger to pause execution of one or all
        // threads.
        // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Pause
        m_dapSession->registerHandler([&](const dap::PauseRequest&) {
            //debugger.pause();
            return dap::PauseResponse();
            });

        // The Continue request instructs the debugger to resume execution of one or
        // all threads.
        // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Continue
        m_dapSession->registerHandler([&](const dap::ContinueRequest&) {
            //debugger.run();
            return dap::ContinueResponse();
            });

        // The Next request instructs the debugger to single line step for a specific
        // thread.
        // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Next
        m_dapSession->registerHandler([&](const dap::NextRequest&) {
            //debugger.stepForward();
            return dap::NextResponse();
            });

        // The StepIn request instructs the debugger to step-in for a specific thread.
        // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_StepIn
        m_dapSession->registerHandler([&](const dap::StepInRequest&) {
            // Step-in treated as step-over as there's only one stack frame.
            //debugger.stepForward();
            return dap::StepInResponse();
            });

        // The StepOut request instructs the debugger to step-out for a specific
        // thread.
        // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_StepOut
        m_dapSession->registerHandler([&](const dap::StepOutRequest&) {
            // Step-out is not supported as there's only one stack frame.
            return dap::StepOutResponse();
            });

        // The SetBreakpoints request instructs the debugger to clear and set a number
        // of line breakpoints for a specific source file.
        // This example debugger only exposes a single source file.
        // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_SetBreakpoints
        m_dapSession->registerHandler([&](const dap::SetBreakpointsRequest& request) {
            dap::SetBreakpointsResponse response;

            auto breakpoints = request.breakpoints.value({});
            if (request.source.sourceReference.value(0) == sourceReferenceId) {
                //debugger.clearBreakpoints();
                response.breakpoints.resize(breakpoints.size());
                for (size_t i = 0; i < breakpoints.size(); i++) {
                    //debugger.addBreakpoint(breakpoints[i].line);
                    //response.breakpoints[i].verified = breakpoints[i].line < numSourceLines;
                    response.breakpoints[i].verified = true;
                }
            }
            else {
                response.breakpoints.resize(breakpoints.size());
            }

            return response;
            });

        // The SetExceptionBreakpoints request configures the debugger's handling of
        // thrown exceptions.
        // This example debugger does not use any exceptions, so this is a no-op.
        // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_SetExceptionBreakpoints
        m_dapSession->registerHandler([&](const dap::SetExceptionBreakpointsRequest&) {
            return dap::SetExceptionBreakpointsResponse();
            });

        // The Source request retrieves the source code for a given source file.
        // This example debugger only exposes one synthetic source file.
        // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Source
        m_dapSession->registerHandler([&](const dap::SourceRequest& request)
            -> dap::ResponseOrError<dap::SourceResponse> {
                if (request.sourceReference != sourceReferenceId) {
                    return dap::Error("Unknown source reference '%d'",
                        int(request.sourceReference));
                }

                dap::SourceResponse response;
                response.content = "test";
                return response;
            });

        // The Launch request is made when the client instructs the debugger adapter
        // to start the debuggee. This request contains the launch arguments.
        // This example debugger does nothing with this request.
        // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Launch
        m_dapSession->registerHandler(
            [&](const dap::LaunchRequest&) { return dap::LaunchResponse(); });

        // Handler for disconnect requests
        m_dapSession->registerHandler([&](const dap::DisconnectRequest& request) {
            if (request.terminateDebuggee.value(false)) {
                //terminate.fire();

            }
            if (m_dapLog)
            {
                dap::writef(m_dapLog, "\ndap::Session disconnecting\n");
            }
            return dap::DisconnectResponse();
            });

        // The ConfigurationDone request is made by the client once all configuration
        // requests have been made.
        // This example debugger uses this request to 'start' the debugger.
        // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_ConfigurationDone
        m_dapSession->registerHandler([&](const dap::ConfigurationDoneRequest&) {
            //configured.fire();
            dap::ThreadEvent threadStartedEvent;

            if (m_dapLog)
            {
                dap::writef(m_dapLog, "\ndap::Session started\n");
            }

            threadStartedEvent.reason = "started";
            threadStartedEvent.threadId = threadId;
            m_dapSession->send(threadStartedEvent);
            return dap::ConfigurationDoneResponse();
            });

#ifdef AZ_PLATFORM_WINDOWS 
        // Change stdin & stdout from text mode to binary mode.
        // This ensures sequences of \r\n are not changed to \n.
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif  // OS_WINDOWS

        // All the handlers we care about have now been registered.
        // We now bind the session to stdin and stdout to connect to the client.
        // After the call to bind() we should start receiving requests, starting with
        // the Initialize request.
        std::shared_ptr<dap::Reader> in = dap::file(stdin, false);
        std::shared_ptr<dap::Writer> out = dap::file(stdout, false);

        if (m_dapLog) {
            m_dapSession->bind(spy(in, m_dapLog), spy(out, m_dapLog));
            //dap::writef(m_dapLog, "dap::Session binding\n");
        }
        else {
            m_dapSession->bind(in, out);
        }
    }

    LUADebuggerComponent::~LUADebuggerComponent()
    {
        m_dapSession.reset();
        if (m_dapLog)
        {
            m_dapLog->close();
        }
    }

    void LUADebuggerComponent::GetProvidedServices([[maybe_unused]]AZ::ComponentDescriptor::DependencyArrayType& provided)
    {

    }

    void LUADebuggerComponent::GetIncompatibleServices([[maybe_unused]]AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {

    }

    void LUADebuggerComponent::GetRequiredServices([[maybe_unused]]AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        //required.push_back(AZ_CRC_CE("RemoteToolsService"));
    }

    void LUADebuggerComponent::GetDependentServices([[maybe_unused]]AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        //dependent.push_back(AZ_CRC_CE("RemoteToolsService"));
    }

    //////////////////////////////////////////////////////////////////////////
    // AZ::Component
    void LUADebuggerComponent::Init()
    {
    }

    void LUADebuggerComponent::Activate()
    {
        LUADebuggerRequestBus::Handler::BusConnect();
        AZ::SystemTickBus::Handler::BusConnect();
    }

    void LUADebuggerComponent::Deactivate()
    {
        AZ::SystemTickBus::Handler::BusDisconnect();
        LUADebuggerRequestBus::Handler::BusDisconnect();
        m_remoteTools = nullptr;
    }

    void LUADebuggerComponent::OnSystemTick()
    {
        if (!m_remoteTools)
        {
            m_remoteTools = AzFramework::RemoteToolsInterface::Get();
            return;
        }

        const AzFramework::ReceivedRemoteToolsMessages* messages = m_remoteTools->GetReceivedMessages(AzFramework::LuaToolsKey);
        if (!messages)
        {
            return;
        }

        // handle messages recevied from the editor
        for (const AzFramework::RemoteToolsMessagePointer& msg : *messages)
        {
            if (AzFramework::ScriptDebugAck* ack = azdynamic_cast<AzFramework::ScriptDebugAck*>(msg.get()))
            {
                if (ack->m_ackCode == AZ_CRC_CE("Ack"))
                {
                    if (ack->m_request == AZ_CRC_CE("Continue") || ack->m_request == AZ_CRC_CE("StepIn") ||
                        ack->m_request == AZ_CRC_CE("StepOut") || ack->m_request == AZ_CRC_CE("StepOver"))
                    {
                        //LUAEditor::Context_DebuggerManagement::Bus::Broadcast(
                        //    &LUAEditor::Context_DebuggerManagement::OnExecutionResumed);
                    }
                    else if (ack->m_request == AZ_CRC_CE("AttachDebugger"))
                    {
                        //LUAEditor::Context_DebuggerManagement::Bus::Broadcast(
                        //    &LUAEditor::Context_DebuggerManagement::OnDebuggerAttached);
                    }
                    else if (ack->m_request == AZ_CRC_CE("DetachDebugger"))
                    {
                        //LUAEditor::Context_DebuggerManagement::Bus::Broadcast(
                        //    &LUAEditor::Context_DebuggerManagement::OnDebuggerDetached);
                    }
                    AZ_TracePrintf(
                        "LUA Debug",
                        "Debug Agent: Ack 0x%x.\n",
                        ack->m_request);
                }
                else if (ack->m_ackCode == AZ_CRC_CE("IllegalOperation"))
                {
                    if (ack->m_request == AZ_CRC_CE("ExecuteScript"))
                    {
                        //LUAEditor::Context_DebuggerManagement::Bus::Broadcast(
                        //    &LUAEditor::Context_DebuggerManagement::OnExecuteScriptResult, false);
                    }
                    else if (ack->m_request == AZ_CRC_CE("AttachDebugger"))
                    {
                        //LUAEditor::Context_DebuggerManagement::Bus::Broadcast(
                        //    &LUAEditor::Context_DebuggerManagement::OnDebuggerRefused);
                    }
                    else
                    {
                        AZ_TracePrintf(
                            "LUA Debug",
                            "Debug Agent: Illegal operation 0x%x. Script context is in the wrong state.\n",
                            ack->m_request);
                    }
                }
                else if (ack->m_ackCode == AZ_CRC_CE("AccessDenied"))
                {
                    AZ_TracePrintf("LUA Debug", "Debug Agent: Access denied 0x%x. Attach debugger first!\n", ack->m_request);
                    //LUAEditor::Context_DebuggerManagement::Bus::Broadcast(
                    //    &LUAEditor::Context_DebuggerManagement::OnDebuggerDetached);
                }
                else if (ack->m_ackCode == AZ_CRC_CE("InvalidCmd"))
                {
                    AZ_TracePrintf(
                        "LUA Debug",
                        "The remote script debug agent claims that we sent it an invalid request(0x%x)!\n",
                        ack->m_request);
                }
            }
            else if (azrtti_istypeof<AzFramework::ScriptDebugAckBreakpoint*>(msg.get()))
            {
                AzFramework::ScriptDebugAckBreakpoint* ackBreakpoint =
                    azdynamic_cast<AzFramework::ScriptDebugAckBreakpoint*>(msg.get());
                if (ackBreakpoint->m_id == AZ_CRC_CE("BreakpointHit"))
                {
                    //LUAEditor::Context_DebuggerManagement::Bus::Broadcast(
                    //    &LUAEditor::Context_DebuggerManagement::OnBreakpointHit,
                    //    ackBreakpoint->m_moduleName,
                    //    ackBreakpoint->m_line);
                }
                else if (ackBreakpoint->m_id == AZ_CRC_CE("AddBreakpoint"))
                {
                    //LUAEditor::Context_DebuggerManagement::Bus::Broadcast(
                    //    &LUAEditor::Context_DebuggerManagement::OnBreakpointAdded,
                    //    ackBreakpoint->m_moduleName,
                    //    ackBreakpoint->m_line);
                }
                else if (ackBreakpoint->m_id == AZ_CRC_CE("RemoveBreakpoint"))
                {
                    //LUAEditor::Context_DebuggerManagement::Bus::Broadcast(
                    //    &LUAEditor::Context_DebuggerManagement::OnBreakpointRemoved,
                    //    ackBreakpoint->m_moduleName,
                    //    ackBreakpoint->m_line);
                }
                AZ_TracePrintf(
                    "LUA Debug",
                    "Debug Agent: Ack breakpoint 0x%x.\n",
                    ackBreakpoint->m_id);
            }
            else if (
                AzFramework::ScriptDebugAckExecute* ackExecute = azdynamic_cast<AzFramework::ScriptDebugAckExecute*>(msg.get()))
            {
                //LUAEditor::Context_DebuggerManagement::Bus::Broadcast(
                //    &LUAEditor::Context_DebuggerManagement::OnExecuteScriptResult, ackExecute->m_result);
            }
            else if (azrtti_istypeof<AzFramework::ScriptDebugEnumLocalsResult*>(msg.get()))
            {
                //AzFramework::ScriptDebugEnumLocalsResult* enumLocals =
                //    azdynamic_cast<AzFramework::ScriptDebugEnumLocalsResult*>(msg.get());
                //LUAEditor::Context_DebuggerManagement::Bus::Broadcast(
                //    &LUAEditor::Context_DebuggerManagement::OnReceivedLocalVariables, enumLocals->m_names);
            }
            else if (azrtti_istypeof<AzFramework::ScriptDebugEnumContextsResult*>(msg.get()))
            {
                //AzFramework::ScriptDebugEnumContextsResult* enumContexts =
                //    azdynamic_cast<AzFramework::ScriptDebugEnumContextsResult*>(msg.get());
                //LUAEditor::Context_DebuggerManagement::Bus::Broadcast(
                //    &LUAEditor::Context_DebuggerManagement::OnReceivedAvailableContexts, enumContexts->m_names);
            }
            else if (azrtti_istypeof<AzFramework::ScriptDebugGetValueResult*>(msg.get()))
            {
                //AzFramework::ScriptDebugGetValueResult* getValues =
                //    azdynamic_cast<AzFramework::ScriptDebugGetValueResult*>(msg.get());
                //LUAEditor::Context_DebuggerManagement::Bus::Broadcast(
                //    &LUAEditor::Context_DebuggerManagement::OnReceivedValueState, getValues->m_value);
            }
            else if (azrtti_istypeof<AzFramework::ScriptDebugSetValueResult*>(msg.get()))
            {
                //AzFramework::ScriptDebugSetValueResult* setValue =
                //    azdynamic_cast<AzFramework::ScriptDebugSetValueResult*>(msg.get());
                //LUAEditor::Context_DebuggerManagement::Bus::Broadcast(
                //    &LUAEditor::Context_DebuggerManagement::OnSetValueResult, setValue->m_name, setValue->m_result);
            }
            else if (azrtti_istypeof<AzFramework::ScriptDebugCallStackResult*>(msg.get()))
            {
                AzFramework::ScriptDebugCallStackResult* callStackResult =
                    azdynamic_cast<AzFramework::ScriptDebugCallStackResult*>(msg.get());
                AZStd::vector<AZStd::string> callstack;
                const char* c1 = callStackResult->m_callstack.c_str();
                for (const char* c2 = c1; *c2; ++c2)
                {
                    if (*c2 == '\n')
                    {
                        callstack.emplace_back().assign(c1, c2);
                        c1 = c2 + 1;
                    }
                }
                callstack.emplace_back() = c1;
                //LUAEditor::Context_DebuggerManagement::Bus::Broadcast(
                //    &LUAEditor::Context_DebuggerManagement::OnReceivedCallstack, callstack);
            }
            else if (azrtti_istypeof<AzFramework::ScriptDebugRegisteredGlobalsResult*>(msg.get()))
            {
                //AzFramework::ScriptDebugRegisteredGlobalsResult* registeredGlobals =
                //    azdynamic_cast<AzFramework::ScriptDebugRegisteredGlobalsResult*>(msg.get());
                //LUAEditor::Context_DebuggerManagement::Bus::Broadcast(
                //    &LUAEditor::Context_DebuggerManagement::OnReceivedRegisteredGlobals,
                //    registeredGlobals->m_methods,
                //    registeredGlobals->m_properties);
            }
            else if (azrtti_istypeof<AzFramework::ScriptDebugRegisteredClassesResult*>(msg.get()))
            {
                //AzFramework::ScriptDebugRegisteredClassesResult* registeredClasses =
                //    azdynamic_cast<AzFramework::ScriptDebugRegisteredClassesResult*>(msg.get());
                //LUAEditor::Context_DebuggerManagement::Bus::Broadcast(
                //    &LUAEditor::Context_DebuggerManagement::OnReceivedRegisteredClasses, registeredClasses->m_classes);
            }
            else if (azrtti_istypeof<AzFramework::ScriptDebugRegisteredEBusesResult*>(msg.get()))
            {
                //AzFramework::ScriptDebugRegisteredEBusesResult* registeredEBuses =
                //    azdynamic_cast<AzFramework::ScriptDebugRegisteredEBusesResult*>(msg.get());
                //LUAEditor::Context_DebuggerManagement::Bus::Broadcast(
                //    &LUAEditor::Context_DebuggerManagement::OnReceivedRegisteredEBuses, registeredEBuses->m_ebusList);
            }
            else
            {
                AZ_Assert(false, "We received a message of an unrecognized class type!");
            }
        }
        m_remoteTools->ClearReceivedMessages(AzFramework::LuaToolsKey);
    }

    void LUADebuggerComponent::EnumerateContexts()
    {
        AZ_TracePrintf("LUA Debug", "LUADebuggerComponent::EnumerateContexts()\n");

        AzFramework::RemoteToolsEndpointInfo targetInfo;
        if (m_remoteTools && GetDesiredTarget(targetInfo))
        {
            m_remoteTools->SendRemoteToolsMessage(targetInfo, AzFramework::ScriptDebugRequest(AZ_CRC_CE("EnumContexts")));
        }
    }

    void LUADebuggerComponent::AttachDebugger(const char* scriptContextName)
    {
        AZ_TracePrintf("LUA Debug", "LUADebuggerComponent::AttachDebugger( %s )\n", scriptContextName);

        AZ_Assert(scriptContextName, "You need to supply a valid script context name to attach to!");
        AzFramework::RemoteToolsEndpointInfo targetInfo;
        if (m_remoteTools && GetDesiredTarget(targetInfo))
        {
            m_remoteTools->SendRemoteToolsMessage(targetInfo, AzFramework::ScriptDebugRequest(AZ_CRC_CE("AttachDebugger"), scriptContextName));
        }
    }

    void LUADebuggerComponent::DetachDebugger()
    {
        AZ_TracePrintf("LUA Debug", "LUADebuggerComponent::DetachDebugger()\n");

        AzFramework::RemoteToolsEndpointInfo targetInfo;
        if (m_remoteTools && GetDesiredTarget(targetInfo))
        {
            m_remoteTools->SendRemoteToolsMessage(targetInfo, AzFramework::ScriptDebugRequest(AZ_CRC_CE("DetachDebugger")));
        }
    }

    void LUADebuggerComponent::EnumRegisteredClasses(const char* scriptContextName)
    {
        AzFramework::RemoteToolsEndpointInfo targetInfo;
        if (m_remoteTools && GetDesiredTarget(targetInfo))
        {
            m_remoteTools->SendRemoteToolsMessage(targetInfo, AzFramework::ScriptDebugRequest(AZ_CRC_CE("EnumRegisteredClasses"), scriptContextName));
        }
    }

    void LUADebuggerComponent::EnumRegisteredEBuses(const char* scriptContextName)
    {
        AzFramework::RemoteToolsEndpointInfo targetInfo;
        if (m_remoteTools && GetDesiredTarget(targetInfo))
        {
            m_remoteTools->SendRemoteToolsMessage(targetInfo, AzFramework::ScriptDebugRequest(AZ_CRC_CE("EnumRegisteredEBuses"), scriptContextName));
        }
    }

    void LUADebuggerComponent::EnumRegisteredGlobals(const char* scriptContextName)
    {
        AzFramework::RemoteToolsEndpointInfo targetInfo;
        if (m_remoteTools && GetDesiredTarget(targetInfo))
        {
            m_remoteTools->SendRemoteToolsMessage(targetInfo, AzFramework::ScriptDebugRequest(AZ_CRC_CE("EnumRegisteredGlobals"), scriptContextName));
        }
    }

    void LUADebuggerComponent::CreateBreakpoint(const AZStd::string& debugName, int lineNumber)
    {
        // register a breakpoint.

        // Debug name will be the full, absolute path, so convert it to the relative path
        AZStd::string relativePath = debugName;
        AzToolsFramework::AssetSystemRequestBus::Broadcast(
            &AzToolsFramework::AssetSystemRequestBus::Events::GetRelativeProductPathFromFullSourceOrProductPath, debugName, relativePath);
        relativePath = "@" + relativePath;

        AzFramework::RemoteToolsEndpointInfo targetInfo;
        if (m_remoteTools && GetDesiredTarget(targetInfo))
        {
            // local editors are never debuggable (they'd never have the debuggable flag) so if you get here you know its over the network
            // and its network id is targetID.
            m_remoteTools->SendRemoteToolsMessage(targetInfo, AzFramework::ScriptDebugBreakpointRequest(AZ_CRC_CE("AddBreakpoint"), relativePath.c_str(), static_cast<AZ::u32>(lineNumber)));
        }
    }

    void LUADebuggerComponent::RemoveBreakpoint(const AZStd::string& debugName, int lineNumber)
    {
        // remove a breakpoint.

        // Debug name will be the full, absolute path, so convert it to the relative path
        AZStd::string relativePath = debugName;
        AzToolsFramework::AssetSystemRequestBus::Broadcast(
            &AzToolsFramework::AssetSystemRequestBus::Events::GetRelativeProductPathFromFullSourceOrProductPath, debugName, relativePath);
        relativePath = "@" + relativePath;

        AzFramework::RemoteToolsEndpointInfo targetInfo;
        if (m_remoteTools && GetDesiredTarget(targetInfo))
        {
            // local editors are never debuggable (they'd never have the debuggable flag) so if you get here you know its over the network
            // and its network id is targetID.
            m_remoteTools->SendRemoteToolsMessage(targetInfo, AzFramework::ScriptDebugBreakpointRequest(AZ_CRC_CE("RemoveBreakpoint"), relativePath.c_str(), static_cast<AZ::u32>(lineNumber)));
        }
    }

    void LUADebuggerComponent::DebugRunStepOver()
    {
        AzFramework::RemoteToolsEndpointInfo targetInfo;
        if (m_remoteTools && GetDesiredTarget(targetInfo))
        {
            m_remoteTools->SendRemoteToolsMessage(targetInfo, AzFramework::ScriptDebugRequest(AZ_CRC_CE("StepOver")));
        }
    }

    void LUADebuggerComponent::DebugRunStepIn()
    {
        AzFramework::RemoteToolsEndpointInfo targetInfo;
        if (m_remoteTools && GetDesiredTarget(targetInfo))
        {
            m_remoteTools->SendRemoteToolsMessage(targetInfo, AzFramework::ScriptDebugRequest(AZ_CRC_CE("StepIn")));
        }
    }

    void LUADebuggerComponent::DebugRunStepOut()
    {
        AzFramework::RemoteToolsEndpointInfo targetInfo;
        if (m_remoteTools && GetDesiredTarget(targetInfo))
        {
            m_remoteTools->SendRemoteToolsMessage(targetInfo, AzFramework::ScriptDebugRequest(AZ_CRC_CE("StepOut")));
        }
    }

    void LUADebuggerComponent::DebugRunStop()
    {
        // Script context can't be stopped. What should we do here?
    }

    void LUADebuggerComponent::DebugRunContinue()
    {
        AzFramework::RemoteToolsEndpointInfo targetInfo;
        if (m_remoteTools && GetDesiredTarget(targetInfo))
        {
            m_remoteTools->SendRemoteToolsMessage(targetInfo, AzFramework::ScriptDebugRequest(AZ_CRC_CE("Continue")));
        }
    }

    void LUADebuggerComponent::EnumLocals()
    {
        AzFramework::RemoteToolsEndpointInfo targetInfo;
        if (m_remoteTools && GetDesiredTarget(targetInfo))
        {
            m_remoteTools->SendRemoteToolsMessage(targetInfo, AzFramework::ScriptDebugRequest(AZ_CRC_CE("EnumLocals")));
        }
    }

    void LUADebuggerComponent::GetValue(const AZStd::string& varName)
    {
        AzFramework::RemoteToolsEndpointInfo targetInfo;
        if (m_remoteTools && GetDesiredTarget(targetInfo))
        {
            m_remoteTools->SendRemoteToolsMessage(targetInfo, AzFramework::ScriptDebugRequest(AZ_CRC_CE("GetValue"), varName.c_str()));
        }
    }

    void LUADebuggerComponent::SetValue(const AZ::ScriptContextDebug::DebugValue& value)
    {
        (void)value;
        AzFramework::RemoteToolsEndpointInfo targetInfo;
        if (m_remoteTools && GetDesiredTarget(targetInfo))
        {
            AzFramework::ScriptDebugSetValue request;
            request.m_value = value;
            m_remoteTools->SendRemoteToolsMessage(targetInfo, request);
        }
    }

    void LUADebuggerComponent::GetCallstack()
    {
        AzFramework::RemoteToolsEndpointInfo targetInfo;
        if (m_remoteTools && GetDesiredTarget(targetInfo))
        {
            m_remoteTools->SendRemoteToolsMessage(targetInfo, AzFramework::ScriptDebugRequest(AZ_CRC_CE("GetCallstack")));
        }
    }

    void LUADebuggerComponent::DesiredTargetChanged(AZ::u32 newTargetID, AZ::u32 oldTargetID)
    {
        (void)newTargetID;
        (void)oldTargetID;
    }

    void LUADebuggerComponent::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<LUADebuggerComponent, AZ::Component>();
        }
    }
}
