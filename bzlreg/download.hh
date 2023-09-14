#pragma once

#include <string_view>
#include <optional>
#include <vector>
#include <cstddef>

namespace bzlreg {
auto download_file( //
	std::string_view url
) -> std::optional<std::vector<std::byte>>;
}
