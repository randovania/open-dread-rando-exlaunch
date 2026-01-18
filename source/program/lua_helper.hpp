#pragma once

#include <utility>

#include "lib/nx/kernel/svc.h"
#include "lua-5.1.5/src/lua.hpp"
#include "dread_types.hpp"

namespace odr::lua {

	typedef bool (*CallFunctionWithArgumentsFn)(const char *functionName, const char *argumentTypes, ...);

	void InstallFunctions(functionOffsets* offsets);
	CallFunctionWithArgumentsFn GetCallFunctionWithArgumentsPtr();

	template <typename ...Params>
	bool CallFunctionWithArguments(const char* functionName, const char* argumentTypes, Params&&... params) {
		CallFunctionWithArgumentsFn callFunctionWithArgumentsPtr = GetCallFunctionWithArgumentsPtr();

		if (callFunctionWithArgumentsPtr == NULL) {
			svcOutputDebugString("CallFunctionWithArguments could not be found!", 45);
			return false;
		}

		return callFunctionWithArgumentsPtr(functionName, argumentTypes, std::forward<Params>(params)...);
	}

};