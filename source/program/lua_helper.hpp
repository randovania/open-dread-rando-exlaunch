#pragma once

#include <utility>

#include "lib/nx/kernel/svc.h"
#include "lua-5.1.5/src/lua.hpp"
#include "dread_types.hpp"

namespace odr::lua {

	typedef bool (*CallFunctionWithArgumentsFn)(const char *functionName, const char *argumentTypes, ...);

	void InstallFunctions(functionOffsets* offsets);
	CallFunctionWithArgumentsFn GetCallFunctionWithArgumentsPtr();

	/**
	 * Calls the Lua function named `functionName` with zero or more arguments, whose types are indicated by the `argumentTypes` format string.
	 * \param functionName The name of a Lua function. May be a compound name referring to a global object and function (e.g. `Game.LogWarn`).
	 * \param argumentTypes A string containing one character per argument passed, representing the types of the arguments. The known supported
	 * types are as follows: `b` - boolean, `i` - integer, `d` or `f` - float, `s` - string, `o` - unknown object type, `t` - Lua table,
	 * `v` - vector (instance of `base::math::CVector3D`).
	 * \param params Zero or more arguments, passed as variadic.
	 */
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