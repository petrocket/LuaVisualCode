
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Module/Module.h>
#include <Clients/LuaVSCodeSystemComponent.h>

namespace LuaVSCode
{
    class LuaVSCodeModuleInterface
        : public AZ::Module
    {
    public:
        AZ_RTTI(LuaVSCodeModuleInterface, "{5CC4BE58-A76A-45C1-8883-F336641EA6DB}", AZ::Module);
        AZ_CLASS_ALLOCATOR(LuaVSCodeModuleInterface, AZ::SystemAllocator, 0);

        LuaVSCodeModuleInterface()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            // Add ALL components descriptors associated with this gem to m_descriptors.
            // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and EditContext.
            // This happens through the [MyComponent]::Reflect() function.
            m_descriptors.insert(m_descriptors.end(), {
                LuaVSCodeSystemComponent::CreateDescriptor(),
                });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
                azrtti_typeid<LuaVSCodeSystemComponent>(),
            };
        }
    };
}// namespace LuaVSCode
