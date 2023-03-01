

#include <LuaVSCodeModuleInterface.h>
#include "LuaVSCodeSystemComponent.h"

namespace LuaVSCode
{
    class LuaVSCodeModule
        : public LuaVSCodeModuleInterface
    {
    public:
        AZ_RTTI(LuaVSCodeModule, "{FD78653B-160F-4A16-BC0E-28A332F2091F}", LuaVSCodeModuleInterface);
        AZ_CLASS_ALLOCATOR(LuaVSCodeModule, AZ::SystemAllocator, 0);
    };
}// namespace LuaVSCode

AZ_DECLARE_MODULE_CLASS(Gem_LuaVSCode, LuaVSCode::LuaVSCodeModule)
