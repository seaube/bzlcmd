#pragma once

#include <filesystem>

namespace bzlmod {
auto init_module( //
	std::filesystem::path dir
) -> int;
}
