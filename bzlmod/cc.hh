#pragma once

#include <string_view>

namespace bzlmod {
auto cc_include_deps( //
	std::string_view file_path,
	bool fix
) -> int;

auto cc_list_headers(std::string_view label, bool deps) -> int;
}
