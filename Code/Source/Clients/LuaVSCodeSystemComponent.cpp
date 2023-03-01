
#include "LuaVSCodeSystemComponent.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>

namespace LuaVSCode
{
    void LuaVSCodeSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<LuaVSCodeSystemComponent, AZ::Component>()
                ->Version(0)
                ;

            if (AZ::EditContext* ec = serialize->GetEditContext())
            {
                ec->Class<LuaVSCodeSystemComponent>("LuaVSCode", "[Description of functionality provided by this System Component]")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System"))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ;
            }
        }
    }

    void LuaVSCodeSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("LuaVSCodeService"));
    }

    void LuaVSCodeSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("LuaVSCodeService"));
    }

    void LuaVSCodeSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
    }

    void LuaVSCodeSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
    }

    LuaVSCodeSystemComponent::LuaVSCodeSystemComponent()
    {
        if (LuaVSCodeInterface::Get() == nullptr)
        {
            LuaVSCodeInterface::Register(this);
        }
    }

    LuaVSCodeSystemComponent::~LuaVSCodeSystemComponent()
    {
        if (LuaVSCodeInterface::Get() == this)
        {
            LuaVSCodeInterface::Unregister(this);
        }
    }

    void LuaVSCodeSystemComponent::Init()
    {
    }

    void LuaVSCodeSystemComponent::Activate()
    {
        LuaVSCodeRequestBus::Handler::BusConnect();
        AZ::TickBus::Handler::BusConnect();

    }

    void LuaVSCodeSystemComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        LuaVSCodeRequestBus::Handler::BusDisconnect();
    }

    void LuaVSCodeSystemComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
    }

} // namespace LuaVSCode
