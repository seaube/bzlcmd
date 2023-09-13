#pragma once

#include <vector>
#include <cstddef>

namespace bzlreg {
auto decompress_archive( //
	const std::vector<std::byte>& data
) -> std::vector<std::byte>;
}