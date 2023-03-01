
#include <LuaVSCodeModuleInterface.h>
#include "LuaVSCodeEditorSystemComponent.h"

namespace LuaVSCode
{
    class LuaVSCodeEditorModule
        : public LuaVSCodeModuleInterface
    {
    public:
        AZ_RTTI(LuaVSCodeEditorModule, "{FD78653B-160F-4A16-BC0E-28A332F2091F}", LuaVSCodeModuleInterface);
        AZ_CLASS_ALLOCATOR(LuaVSCodeEditorModule, AZ::SystemAllocator, 0);

        LuaVSCodeEditorModule()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            // Add ALL components descriptors associated with this gem to m_descriptors.
            // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and EditContext.
            // This happens through the [MyComponent]::Reflect() function.
            m_descriptors.insert(m_descriptors.end(), {
                LuaVSCodeEditorSystemComponent::CreateDescriptor(),
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         * Non-SystemComponents should not be added here
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList {
                azrtti_typeid<LuaVSCodeEditorSystemComponent>(),
            };
        }
    };
}// namespace LuaVSCode

AZ_DECLARE_MODULE_CLASS(Gem_LuaVSCode, LuaVSCode::LuaVSCodeEditorModule)
