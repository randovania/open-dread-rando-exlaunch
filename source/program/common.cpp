#include "common.hpp"

#include "lib/util/modules.hpp"
#include "dread_types.hpp"



SStringPool** stringPool = NULL;
ReflectionManager** refMgr = NULL;

crc64_t (*Crc64Ptr)(char const* str, u64 size) = NULL;
void (*GetCStrIdPtr)(CStrId* inst, const char* str, bool storeInPool) = NULL;
void (*DiscardStringInstancePtr)(SStringPool* pool, CStringInstance* inst) = NULL;
CClass* (*GetCClassPtr)(ReflectionManager* refMgr, crc64_t* hash) = NULL;

namespace odr::common {
    void InstallFunctions(functionOffsets *offsets) {
        refMgr = (ReflectionManager**)exl::util::modules::GetTargetOffset(offsets->ReflectionManagerPtr);
        stringPool = (SStringPool**)exl::util::modules::GetTargetOffset(offsets->StringPoolPtr);
        
        Crc64Ptr = (crc64_t(*)(char const*, u64))exl::util::modules::GetTargetOffset(offsets->crc64);
        GetCStrIdPtr = (void(*)(CStrId*, const char*, bool))exl::util::modules::GetTargetOffset(offsets->GetCStrId);
        DiscardStringInstancePtr = (void(*)(SStringPool*, CStringInstance*))exl::util::modules::GetTargetOffset(offsets->DiscardStringInstance);
        GetCClassPtr = (CClass*(*)(ReflectionManager*, crc64_t*))exl::util::modules::GetTargetOffset(offsets->GetCClassPtr);
    }

    /** Takes a char const* and returns its crc64 hash */
    crc64_t CRC64(char const* str) {
        if (Crc64Ptr == NULL)
            return (crc64_t)-1;
        
        return Crc64Ptr(str, strlen(str));
    }

    /** Get the CStrId for the selected string. If the string does not exist in the pool, one will be allocated. */
    void GetCStrId(CStrId* inst, const char* str, bool storeInPool) {
        if (GetCStrIdPtr == NULL)
            return;

        GetCStrIdPtr(inst, str, storeInPool);
    }

    /** Use this on any instance of CStrId you are overwriting/not using. this is an inlined function in the executable. */
    void DiscardCStrId(CStrId* inst) {
        if (*stringPool == NULL)
            return;

        if (inst != NULL && (*stringPool)->nullStr != NULL) {
            inst->value->uses -= 1;
            if (inst->value->uses == 0)
                DiscardStringInstancePtr(*stringPool, inst->value);
        }

        inst->value = (*stringPool)->nullStr;
    }

    /** Retrieves a CClass (possibly CType?) from the reflection manager. */
    CClass* GetCClass(char const* className) {
        if (GetCClassPtr == NULL || *refMgr == NULL)
            return NULL;

        crc64_t hash = CRC64(className);
        return GetCClassPtr(*refMgr, &hash);
    }
}