#pragma once

#include <type_traits>

#include "lib.hpp"
#include "lib/util/modules.hpp"
#include "dread_types.hpp"

/** Game functions that several modules use. Resolved once at startup. */
namespace odr::game {

	/** Returns the address of a function or global at the given offset into the game's main module. */
	template<typename T>
	inline T Resolve(ptrdiff_t offset) {
		return (T) exl::util::modules::GetTargetOffset(offset);
	}

	/**
	 * A GOT line of the game's main module. The loader fills it with the address of a game global.
	 * T is the type of the global itself, e.g. CGame* for g_game or CStrId for a CStrId global.
	 * The line is read on every access.
	 */
	template<typename T>
	class GotSlot {
	public:
		void Install(ptrdiff_t slotOffset) {
			slot = Resolve<T**>(slotOffset);
		}

		/** The global's address, or nullptr if not installed or not filled. */
		T* Address() const {
			return slot != nullptr ? *slot : nullptr;
		}

		/** The global's current value, or nullptr if it can't be read.. */
		T Get() const requires std::is_pointer_v<T> {
			T* address = Address();
			return address != nullptr ? *address : nullptr;
		}

	private:
		T** slot = nullptr;
	};

	/** Calculates the CRC64 hash of a string. */
	using Crc64Fn = u64 (*)(const char* str, u64 size);
	/** Builds a CStrId from a C string. */
	using HashStringFn = void (*)(CStrId* out, const char* str, bool addToPool);

	extern Crc64Fn crc64;
	extern HashStringFn hashString;

	/** Must run before the other modules install. */
	void InstallFunctions();

};
