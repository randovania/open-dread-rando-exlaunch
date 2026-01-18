#include "lua_helper.hpp"
#include "lib.hpp"

namespace odr::lua {

	static CallFunctionWithArgumentsFn callFunctionWithArgumentsPtr;

};

void odr::lua::InstallFunctions(functionOffsets* offsets)  {
	callFunctionWithArgumentsPtr = (bool(*)(const char*, const char*, ...)) exl::util::modules::GetTargetOffset(offsets->CallFunctionWithArguments);
}

odr::lua::CallFunctionWithArgumentsFn odr::lua::GetCallFunctionWithArgumentsPtr() {
	return callFunctionWithArgumentsPtr;
}