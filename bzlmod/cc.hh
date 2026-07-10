#pragma once

#include <string_view>

namespace bzlmod {
auto cc_include_deps( //
	std::string_view file_path,
	bool fix
) -> int;
}
