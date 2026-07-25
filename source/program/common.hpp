#pragma once

#include "lib.hpp"
#include "dread_types.hpp"


namespace odr::common {
    void InstallFunctions(functionOffsets *offsets);

    crc64_t CRC64(char const* str);
    void GetCStrId(CStrId* inst, const char* str, bool storeInPool);
    void DiscardCStrId(CStrId* inst);
    CClass* GetCClass(char const* className);
}