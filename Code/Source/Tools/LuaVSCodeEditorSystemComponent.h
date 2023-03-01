
#pragma once

#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <Clients/LuaVSCodeSystemComponent.h>

namespace LuaVSCode
{
    /// System component for LuaVSCode editor
    class LuaVSCodeEditorSystemComponent
        : public LuaVSCodeSystemComponent
        , protected AzToolsFramework::EditorEvents::Bus::Handler
    {
        using BaseSystemComponent = LuaVSCodeSystemComponent;
    public:
        AZ_COMPONENT(LuaVSCodeEditorSystemComponent, "{4BD43C84-C285-4063-A29B-8FCD0D7D8C4D}", BaseSystemComponent);
        static void Reflect(AZ::ReflectContext* context);

        LuaVSCodeEditorSystemComponent();
        ~LuaVSCodeEditorSystemComponent();

    private:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        // AZ::Component
        void Activate() override;
        void Deactivate() override;
    };
} // namespace LuaVSCode
