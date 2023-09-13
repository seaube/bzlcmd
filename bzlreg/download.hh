#pragma once

#include <string_view>
#include <vector>
#include <cstddef>

namespace bzlreg {
auto download_archive( //
	std::string_view url
) -> std::vector<std::byte>;
}
