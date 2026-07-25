#include "fields.hpp"

#include "lib.hpp"
#include "common.hpp"
#include "dread_types.hpp"
#include "lib/util/modules.hpp"


void* (*RegisterVariablePtr)(CClass *cls, CStrId *varName, CClass *type, uint32_t offset, int64_t getter, int64_t setter) = NULL;


VariableDef g_Variables[0] = {
    // Example:
    // { "CChozoCommanderXLifeComponent",      "fTimeDamaged",         "float",            0x26c }
};


/* Hooks after the reflection system is generated. This allows us to look up the requested cclass/variable type by string.  */
HOOK_DEFINE_TRAMPOLINE(GenerateReflection) {
    static Result Callback(void *reflection) {
        CStrId varName;
        CClass *thisClass;
        CClass *varTypeClass;
        int iVarsMax;


        Result res = Orig(reflection);

        iVarsMax = sizeof(g_Variables) / sizeof(VariableDef);
        for (int i = 0; i < iVarsMax; i++) {
            thisClass = odr::common::GetCClass(g_Variables[i].parentCls);
            odr::common::GetCStrId(&varName, g_Variables[i].varName, true);
            varTypeClass = odr::common::GetCClass(g_Variables[i].varType);

            RegisterVariablePtr(thisClass, &varName, varTypeClass, g_Variables[i].offset, -1, -1);
            odr::common::DiscardCStrId(&varName);
        }

        return res;
    }
};

namespace odr::fields {
    void InstallHooks(functionOffsets* offsets)
    {
        RegisterVariablePtr = (void*(*)(CClass*, CStrId*, CClass*, uint32_t, int64_t, int64_t))exl::util::modules::GetTargetOffset(offsets->RegisterVariable);
        GenerateReflection::InstallAtOffset(offsets->GenerateReflection);
    }
}