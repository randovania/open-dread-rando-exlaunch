#include "lua_helper.hpp"

#include "lib.hpp"
#include "game_functions.hpp"
#include "offsets.hpp"

namespace odr::lua {

	struct Offsets {
		/** Used by the game engine to call a Lua function (by its qualified name) from C++ using varargs arguments. */
		ptrdiff_t CallFunctionWithArguments;
	};

	static constexpr offsets::Versioned<Offsets> OFFSETS = {
		.v1_0_0 = { .CallFunctionWithArguments = 0x790 },
		.v2_1_0 = { .CallFunctionWithArguments = 0x790 }, // wow that's early
	};

	static CallFunctionWithArgumentsFn callFunctionWithArgumentsPtr = nullptr;

};

void odr::lua::InstallFunctions() {
	callFunctionWithArgumentsPtr = game::Resolve<CallFunctionWithArgumentsFn>(OFFSETS.Get().CallFunctionWithArguments);
}

odr::lua::CallFunctionWithArgumentsFn odr::lua::GetCallFunctionWithArgumentsPtr() {
	return callFunctionWithArgumentsPtr;
}
