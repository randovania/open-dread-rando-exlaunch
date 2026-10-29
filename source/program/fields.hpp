#pragma once

#include "lib.hpp"
#include "dread_types.hpp"

struct VariableDef {
    const char* parentCls;
    const char* varName;
    const char* varType;
    uint offset;
};

namespace odr::fields {
    void InstallHooks(functionOffsets *offsets);
}