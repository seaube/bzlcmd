#pragma once

#include <string_view>

namespace bzlmod {
auto add_module( //
	std::string_view dep_name
) -> int;
}
