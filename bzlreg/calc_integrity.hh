#pragma once

#include <filesystem>

namespace bzlreg {
struct calc_integrity_options {
	std::filesystem::path registry_dir;
	std::string           module_name;
};

auto calc_integrity(const calc_integrity_options& options) -> int;
} // namespace bzlreg
