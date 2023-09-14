#pragma once

#include <filesystem>
#include <string_view>

namespace bzlreg {
struct add_module_options {
	std::filesystem::path registry_dir;
	std::string_view      archive_url;
	std::string_view      strip_prefix;
};

auto add_module(add_module_options options) -> int;
} // namespace bzlreg
