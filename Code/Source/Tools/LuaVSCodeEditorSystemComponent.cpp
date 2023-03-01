
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/IO/SystemFile.h>
#include <AzToolsFramework/Script/LuaSymbolsReporterBus.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/RTTI/BehaviorContextUtilities.h>
#include <AzCore/Component/ComponentApplication.h>
#include "LuaVSCodeEditorSystemComponent.h"

namespace LuaVSCode
{
    void LuaVSCodeEditorSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<LuaVSCodeEditorSystemComponent, LuaVSCodeSystemComponent>()
                ->Version(0);
        }
    }

    LuaVSCodeEditorSystemComponent::LuaVSCodeEditorSystemComponent() = default;

    LuaVSCodeEditorSystemComponent::~LuaVSCodeEditorSystemComponent() = default;

    void LuaVSCodeEditorSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        BaseSystemComponent::GetProvidedServices(provided);
        provided.push_back(AZ_CRC_CE("LuaVSCodeEditorService"));
    }

    void LuaVSCodeEditorSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        BaseSystemComponent::GetIncompatibleServices(incompatible);
        incompatible.push_back(AZ_CRC_CE("LuaVSCodeEditorService"));
    }

    void LuaVSCodeEditorSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        BaseSystemComponent::GetRequiredServices(required);
    }

    void LuaVSCodeEditorSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        BaseSystemComponent::GetDependentServices(dependent);
    }

#pragma optimize("", off)
    typedef AZStd::unordered_map<AZ::Uuid, const AzToolsFramework::Script::LuaClassSymbol*> LuaClassUnorderedMap;

    AZStd::string GetArgName(const char* argName, const AZ::BehaviorParameter* arg, const LuaClassUnorderedMap& luaClasses)
    {
        if (arg->m_typeId == AZ::AzTypeInfo<AZStd::string>::Uuid() ||
            arg->m_typeId == AZ::AzTypeInfo<AZStd::basic_string<char, AZStd::char_traits<char>>>::Uuid())
        {
            return { "String" };
        }
        else if (arg->m_typeId == AZ::AzTypeInfo<AZ::u64>::Uuid())
        {
            return { "Number" };
        }

        auto itr = luaClasses.find(arg->m_typeId);
        if (itr == luaClasses.end())
        {
            return AZ::ReplaceCppArtifacts(argName);
        }

        return AZ::ReplaceCppArtifacts(itr->second->m_name.c_str());
    }

    void WriteLuaBehaviorMethod(const AZ::BehaviorMethod* behaviorMethod, AZ::IO::SystemFile& luaMetaFile, [[maybe_unused]] AZ::BehaviorContext* behaviorContext, const char* luaClassName, const char* luaMethodName, const LuaClassUnorderedMap& luaClasses, AZ::BehaviorEBusEventSender* sender = nullptr )
    {
        static AZStd::string params;
        static AZStd::string buffer;
        params = "";
        buffer = "";

        size_t start = 0; // sender&& behaviorMethod == sender->m_event ? 1 : 0;
        for (size_t i = start; i < behaviorMethod->GetNumArguments(); ++i)
        {
            auto arg = behaviorMethod->GetArgument(i);
            auto tooltip = behaviorMethod->GetArgumentToolTip(i);

            //auto itr = luaClasses.find(arg->m_typeId);
            //auto argName = itr != luaClasses.end() ? itr->second->m_name.c_str() : arg->m_name;
            const AZStd::string& luaArgName = GetArgName(arg->m_name, arg, luaClasses);

            if (tooltip != nullptr && !tooltip->empty())
            {
                buffer = AZStd::string::format("---@param %s %s\n", luaArgName.c_str(), tooltip->c_str());
            }
            else
            {
                buffer = AZStd::string::format("---@param %s\n", luaArgName.c_str());
            }
            luaMetaFile.Write(buffer.c_str(), buffer.size());

            if (i > start)
            {
                params.append(", ");
            }
            params.append(luaArgName.c_str());
        }

        if (auto methodResult = behaviorMethod->GetResult())
        {
            const AZStd::string& luaReturnArgName = GetArgName(methodResult->m_name, methodResult, luaClasses);

            //auto itr = luaClasses.find(methodResult->m_typeId);
            //auto returnName = itr != luaClasses.end() ? itr->second->m_name.c_str() : methodResult->m_name;

            //if (auto methodResultClass = behaviorContext->FindClassByTypeId(methodResult->m_typeId); methodResultClass != nullptr)
            //{
            //    buffer = AZStd::string::format("---@return %s %s\n", methodResultClass->m_name.c_str(), methodResult->m_name);
            //    luaMetaFile.Write(buffer.c_str(), buffer.size());
            //}
            // skip void types
            if (!luaReturnArgName.empty() && strcmp(luaReturnArgName.c_str(), "void"))
            {
                buffer = AZStd::string::format("---@return %s\n", luaReturnArgName.c_str());
                luaMetaFile.Write(buffer.c_str(), buffer.size());
            }
        }
        if (sender)
        {
            //if (!strcmp(luaClassName, "UiCursorBus"))
            //{
            //    AZ_TracePrintf("LuaVSCodeEditorSystemComponent", "UiCursorBus");
            //}
            if (sender->m_event == behaviorMethod)
            {
                buffer = AZStd::string::format("function %s.Event.%s(%s) end\n", luaClassName, luaMethodName, params.c_str());
            }
            else
            {
                buffer = AZStd::string::format("function %s.Broadcast.%s(%s) end\n", luaClassName, luaMethodName, params.c_str());
            }
        }
        else
        {
            buffer = AZStd::string::format("function %s.%s(%s) end\n", luaClassName, luaMethodName, params.c_str());
        }
        luaMetaFile.Write(buffer.c_str(), buffer.size());
    }

    void LuaVSCodeEditorSystemComponent::Activate()
    {
        LuaVSCodeSystemComponent::Activate();
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
        if (auto bus = AzToolsFramework::Script::LuaSymbolsReporterRequestBus::FindFirstHandler())
        {
            char luaMetaFilePath[AZ_MAX_PATH_LEN];
            char luaConfigFilePath[AZ_MAX_PATH_LEN];
            AZ::IO::SystemFile luaMetaFile = AZ::IO::SystemFile();
            AZ::IO::SystemFile luaConfigFile = AZ::IO::SystemFile();
            constexpr int openMode = AZ::IO::SystemFile::OpenMode::SF_OPEN_CREATE |
                AZ::IO::SystemFile::OpenMode::SF_OPEN_WRITE_ONLY |
                AZ::IO::SystemFile::OpenMode::SF_OPEN_CREATE_PATH;

            AZ::IO::FileIOBase::GetInstance()->ResolvePath("@projectroot@/scripts/meta/3rd/o3de/config.json", luaConfigFilePath, AZ_MAX_PATH_LEN);
            luaConfigFile.Open(luaConfigFilePath, openMode);
            if (luaConfigFile.IsOpen())
            {
                AZStd::string buffer{ R"({"name" : "O3DE", "words" : ["o3de","ebus"]})" };
                luaConfigFile.Write(buffer.c_str(), buffer.size());
                luaConfigFile.Close();
            }

            AZ::BehaviorContext* behaviorContext = nullptr;
            AZ::ComponentApplicationBus::BroadcastResult(behaviorContext, &AZ::ComponentApplicationBus::Events::GetBehaviorContext);
            AZ_Assert(behaviorContext, "Cannot reflect types without BehaviorContext");

            // get list of classes first so we can make user friendly types for parameters
            const auto& classesList = bus->GetListOfClasses();
            LuaClassUnorderedMap classUuidToLuaClassSymbol;
            classUuidToLuaClassSymbol.reserve(classesList.size());
            for (const auto& luaClass : classesList)
            {
                classUuidToLuaClassSymbol.insert(AZStd::pair{ luaClass.m_typeId, &luaClass });
            }

            // some specific overrides


            AZ::IO::FileIOBase::GetInstance()->ResolvePath("@projectroot@/scripts/meta/3rd/o3de/library/classes.lua", luaMetaFilePath, AZ_MAX_PATH_LEN);
            luaMetaFile.Open(luaMetaFilePath, openMode);
            if (luaMetaFile.IsOpen())
            {
                AZStd::string buffer{ "---@meta\n" };
                luaMetaFile.Write(buffer.c_str(), buffer.size());

                for (const auto& luaClass : classesList)
                {
                    auto behaviorClass = behaviorContext->FindClassByTypeId(luaClass.m_typeId);

                    if (!behaviorClass)
                    {
                        continue;
                    }

                    // consider excluding things excluded from ::List - ::All is already excluded
                    //if (auto attribute = behaviorClass->FindAttribute(AZ::Script::Attributes::ExcludeFrom))
                    //{
                    //    auto excludeAttributeData = azdynamic_cast<const AZ::AttributeData<AZ::Script::Attributes::ExcludeFlags>*>(attribute);
                    //    AZ::u64 exclusionFlags = AZ::Script::Attributes::ExcludeFlags::List;
                    //    bool shouldSkip = (static_cast<AZ::u64>(excludeAttributeData->Get(nullptr)) & exclusionFlags) != 0;
                    //    if (shouldSkip)
                    //    {
                    //        continue;
                    //    }
                    //}

                    buffer = AZStd::string::format("\n---@class %s\n", luaClass.m_name.c_str());
                    luaMetaFile.Write(buffer.c_str(), buffer.size());

                    for (const auto& luaProperties : luaClass.m_properties)
                    {
                        buffer = AZStd::string::format("---@field %s\n", luaProperties.m_name.c_str());
                        luaMetaFile.Write(buffer.c_str(), buffer.size());
                    }

                    buffer = AZStd::string::format("%s = {}\n", luaClass.m_name.c_str());
                    luaMetaFile.Write(buffer.c_str(), buffer.size());

                    for (const auto& luaMethods : luaClass.m_methods)
                    {
                        if (luaMethods.m_name == "AcquireOwnership" || luaMethods.m_name == "ReleaseOwnership")
                        {
                            continue;
                        }
                        if (auto behaviorMethod = behaviorClass->FindMethodByReflectedName(luaMethods.m_name.c_str()))
                        {
                            WriteLuaBehaviorMethod(behaviorMethod, luaMetaFile, behaviorContext, luaClass.m_name.c_str(), luaMethods.m_name.c_str(), classUuidToLuaClassSymbol);

                            /*
                            AZStd::string params;
                            for (size_t i = 0; i < behaviorMethod->GetNumArguments(); ++i)
                            {
                                auto arg = behaviorMethod->GetArgument(i);
                                auto tooltip = behaviorMethod->GetArgumentToolTip(i);

                                if (tooltip != nullptr && !tooltip->empty())
                                {
                                    buffer = AZStd::string::format("---@param %s %s\n", arg->m_name, tooltip->c_str());
                                }
                                else
                                {
                                    buffer = AZStd::string::format("---@param %s\n", arg->m_name);
                                }
                                luaMetaFile.Write(buffer.c_str(), buffer.size());

                                if (i > 0)
                                {
                                    params.append(", ");
                                }
                                params.append(arg->m_name);
                            }

                            if (auto methodResult = behaviorMethod->GetResult())
                            {
                                if (auto methodResultClass = behaviorContext->FindClassByTypeId(methodResult->m_typeId); methodResultClass != nullptr)
                                {
                                    buffer = AZStd::string::format("---@return %s %s\n", methodResultClass->m_name.c_str(), methodResult->m_name);
                                    luaMetaFile.Write(buffer.c_str(), buffer.size());
                                }
                                // skip void types
                                else if(methodResult->m_name && strcmp(methodResult->m_name,"void"))
                                {
                                    buffer = AZStd::string::format("---@return %s\n", methodResult->m_name);
                                    luaMetaFile.Write(buffer.c_str(), buffer.size());
                                }
                            }
                            buffer = AZStd::string::format("function %s.%s(%s) end\n", luaClass.m_name.c_str(), luaMethods.m_name.c_str(), params.c_str());
                            luaMetaFile.Write(buffer.c_str(), buffer.size());
                            */
                        }
                        else
                        {
                            buffer = AZStd::string::format("function %s.%s(%s) end\n", luaClass.m_name.c_str(), luaMethods.m_name.c_str(), luaMethods.m_debugArgumentInfo.c_str());
                            luaMetaFile.Write(buffer.c_str(), buffer.size());
                        }

                    }
                }
                luaMetaFile.Close();
            }

            // EBUSES
            const auto& ebusesList = bus->GetListOfEBuses();
            AZ::IO::FileIOBase::GetInstance()->ResolvePath("@projectroot@/scripts/meta/3rd/o3de/library/ebuses.lua", luaMetaFilePath, AZ_MAX_PATH_LEN);
            luaMetaFile.Open(luaMetaFilePath, openMode);
            if (luaMetaFile.IsOpen())
            {
                AZStd::string buffer{ "---@meta\n" };
                luaMetaFile.Write(buffer.c_str(), buffer.size());

                for (const auto& ebus : ebusesList)
                {
                    auto behaviorEBus = behaviorContext->FindEBusByReflectedName(ebus.m_name);
                    if (!behaviorEBus)
                    {
                        continue;
                    }
                    buffer = AZStd::string::format("---@class %s\n", ebus.m_name.c_str());
                    luaMetaFile.Write(buffer.c_str(), buffer.size());

                    
                    if(ebus.m_senders.empty())
                    { 
                        buffer = AZStd::string::format("%s = {}\n", ebus.m_name.c_str());
                    }
                    else if (ebus.m_canBroadcast)
                    {
                        buffer = AZStd::string::format("%s = { Event = {}, Broadcast = {} }\n", ebus.m_name.c_str());
                    }
                    else
                    {
                        buffer = AZStd::string::format("%s = { Event = {}}\n", ebus.m_name.c_str());
                    }

                    luaMetaFile.Write(buffer.c_str(), buffer.size());

                    //for (const auto& sender : ebus.m_senders)
                    //{
                    //    for (auto senderIt : behaviorEBus->m_events)
                    //    {
                    //        auto behaviorSender = senderIt.second;
                    //        if (behaviorSender.m_event && !strcmp(behaviorSender.m_event->m_name.c_str(), sender.m_name.c_str()))
                    //        {

                    //        }
                    //    }

                    //    buffer = AZStd::string::format("function %s.%s.%s(%s)\n", ebus.m_name.c_str(), sender.m_category.c_str(), sender.m_name.c_str(), sender.m_debugArgumentInfo.c_str());
                    //    luaMetaFile.Write(buffer.c_str(), buffer.size());
                    //}

                    for (auto senderIt : behaviorEBus->m_events)
                    {
                        auto sender = senderIt.second;

                        const char* luaMethodName = nullptr;
                        if (sender.m_event)
                        {
                            for (const auto& luaSenders : ebus.m_senders)
                            {
                                if (sender.m_event && !strcmp(sender.m_event->m_name.c_str(), luaSenders.m_name.c_str()))
                                {
                                    luaMethodName = luaSenders.m_name.c_str();
                                    break;
                                }
                            }
                            WriteLuaBehaviorMethod(sender.m_event, luaMetaFile, behaviorContext, ebus.m_name.c_str(), luaMethodName, classUuidToLuaClassSymbol, &sender);
                        }

                        if (sender.m_broadcast)
                        {
                            if (!luaMethodName)
                            {
                                for (const auto& luaSenders : ebus.m_senders)
                                {
                                    if (sender.m_broadcast && !strcmp(sender.m_broadcast->m_name.c_str(), luaSenders.m_name.c_str()))
                                    {
                                        luaMethodName = luaSenders.m_name.c_str();
                                        break;
                                    }
                                }
                            }
                            WriteLuaBehaviorMethod(sender.m_broadcast, luaMetaFile, behaviorContext, ebus.m_name.c_str(), luaMethodName, classUuidToLuaClassSymbol, &sender);
                        }

                        /*
                        AZStd::string params;
                        AZStd::string paramsMeta;

                        // skip first argument because it is the bus id 
                        for (size_t i = 1; i < senderMethod->GetNumArguments(); ++i)
                        {
                            auto arg = senderMethod->GetArgument(i);
                            auto tooltip = senderMethod->GetArgumentToolTip(i);

                            if (tooltip != nullptr && !tooltip->empty())
                            {
                                paramsMeta.append(AZStd::string::format("---@param %s %s\n", arg->m_name, tooltip->c_str()));
                            }
                            else
                            {
                                paramsMeta.append(AZStd::string::format("---@param %s\n", arg->m_name));
                            }

                            if (i > 1)
                            {
                                params.append(", ");
                            }
                            params.append(arg->m_name);
                        }

                        AZStd::string eventParamsMeta = behaviorEBus->m_idParam.m_name;
                        eventParamsMeta.append(", ").append(paramsMeta);
                        luaMetaFile.Write(eventParamsMeta.c_str(), eventParamsMeta.size());

                        AZStd::string returnMeta;
                        if (auto methodResult = senderMethod->GetResult(); methodResult != nullptr)
                        {
                            if (auto methodResultClass = behaviorContext->FindClassByTypeId(methodResult->m_typeId); methodResultClass != nullptr)
                            {
                                returnMeta = AZStd::string::format("---@return %s %s\n", methodResultClass->m_name.c_str(), methodResult->m_name);
                            }
                            // skip void types
                            else if(methodResult->m_name && strcmp(methodResult->m_name,"void"))
                            {
                                buffer = AZStd::string::format("---@return %s\n", methodResult->m_name);
                                luaMetaFile.Write(buffer.c_str(), buffer.size());
                            }
                        }

                        luaMetaFile.Write(returnMeta.c_str(), returnMeta.size());

                        AZStd::string eventParams = behaviorEBus->m_idParam.m_name;
                        eventParams.append(", ").append(params);
                        buffer = AZStd::string::format("function %s.Event.%s(%s)\n", ebus.m_name.c_str(), sender.m_event->m_name.c_str(), eventParams.c_str());
                        luaMetaFile.Write(buffer.c_str(), buffer.size());

                        if (ebus.m_canBroadcast)
                        {
                            luaMetaFile.Write(paramsMeta.c_str(), paramsMeta.size());
                            luaMetaFile.Write(returnMeta.c_str(), returnMeta.size());
                            buffer = AZStd::string::format("function %s.Broadcast.%s(%s)\n", ebus.m_name.c_str(), sender.m_event->m_name.c_str(), params.c_str());
                            luaMetaFile.Write(buffer.c_str(), buffer.size());
                        }
                        */
                    }

                    //for (const auto& sender : ebus.m_senders)
                    //{
                    //    buffer = AZStd::string::format("function %s.%s.%s(%s)\n", ebus.m_name.c_str(), sender.m_category.c_str(), sender.m_name.c_str(), sender.m_debugArgumentInfo.c_str());
                    //    luaMetaFile.Write(buffer.c_str(), buffer.size());
                    //}
                }

                luaMetaFile.Close();
            }



            // Global properties 
            const auto& globalProperties = bus->GetListOfGlobalProperties();
            AZ::IO::FileIOBase::GetInstance()->ResolvePath("@projectroot@/scripts/meta/3rd/o3de/library/properties.lua", luaMetaFilePath, AZ_MAX_PATH_LEN);
            luaMetaFile.Open(luaMetaFilePath, openMode);
            if (luaMetaFile.IsOpen())
            {
                AZStd::string buffer{ "---@meta\n" };
                luaMetaFile.Write(buffer.c_str(), buffer.size());

                for (const auto& globalProperty : globalProperties)
                {
                    buffer = AZStd::string::format("---@class %s\n", globalProperty.m_name.c_str());
                    luaMetaFile.Write(buffer.c_str(), buffer.size());
                }
                luaMetaFile.Close();
            }

            // Global functions
            const auto& globalFunctions = bus->GetListOfGlobalFunctions();
            AZ::IO::FileIOBase::GetInstance()->ResolvePath("@projectroot@/scripts/meta/3rd/o3de/library/functions.lua", luaMetaFilePath, AZ_MAX_PATH_LEN);
            luaMetaFile.Open(luaMetaFilePath, openMode);
            if (luaMetaFile.IsOpen())
            {
                AZStd::string buffer{ "---@meta\n" };
                luaMetaFile.Write(buffer.c_str(), buffer.size());

                for (const auto& globalFunction : globalFunctions)
                {
                    //buffer = AZStd::string::format("---@class %s\n", globalFunction.m_name.c_str());
                    buffer = AZStd::string::format("function %s(%s) end\n", globalFunction.m_name.c_str(), globalFunction.m_debugArgumentInfo.c_str());
                    luaMetaFile.Write(buffer.c_str(), buffer.size());
                }
                luaMetaFile.Close();
            }
        }
    }
#pragma optimize("", on)

    void LuaVSCodeEditorSystemComponent::Deactivate()
    {
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        LuaVSCodeSystemComponent::Deactivate();
    }

} // namespace LuaVSCode
