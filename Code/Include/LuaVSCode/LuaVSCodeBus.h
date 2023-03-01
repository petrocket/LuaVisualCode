
#pragma once

#include <AzCore/EBus/EBus.h>
#include <AzCore/Interface/Interface.h>

namespace LuaVSCode
{
    class LuaVSCodeRequests
    {
    public:
        AZ_RTTI(LuaVSCodeRequests, "{1D94E60C-4EAF-4A01-B10B-79C98E3C6A40}");
        virtual ~LuaVSCodeRequests() = default;
        // Put your public methods here
    };
    
    class LuaVSCodeBusTraits
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////
    };

    using LuaVSCodeRequestBus = AZ::EBus<LuaVSCodeRequests, LuaVSCodeBusTraits>;
    using LuaVSCodeInterface = AZ::Interface<LuaVSCodeRequests>;

} // namespace LuaVSCode
