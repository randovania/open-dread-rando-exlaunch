#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <type_traits>

/**
 * Per-version offsets. Each module defines its own offset struct and a Versioned<> table
 * next to the code that uses it, so no central file grows with every new feature.
 */
namespace odr::offsets {

	enum class GameVersion {
		V1_0_0, // also 2.0.0
		V2_1_0,
	};

	/** Reads the game's display version on first use. Unknown versions count as 1.0.0. */
	GameVersion GetGameVersion();

	/** One offset struct per supported game version. */
	template<typename T>
	struct Versioned {
		T v1_0_0;
		T v2_1_0;

		const T& Get() const {
			return GetGameVersion() == GameVersion::V2_1_0 ? v2_1_0 : v1_0_0;
		}
	};

	/** True if every field of an offset struct is set (non-zero). Offset structs must only contain ptrdiff_t fields. */
	template<typename T>
	constexpr bool HasOffsets(const T& offsets) {
		static_assert(std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>);
		static_assert(sizeof(T) % sizeof(ptrdiff_t) == 0, "offset structs must only contain ptrdiff_t fields");

		auto fields = std::bit_cast<std::array<ptrdiff_t, sizeof(T) / sizeof(ptrdiff_t)>>(offsets);
		for (ptrdiff_t field : fields) {
			if (field == 0) {
				return false;
			}
		}
		return true;
	}

};
