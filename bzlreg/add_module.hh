#pragma once

#include <filesystem>
#include <string_view>
#include <vector>
#include <string>

namespace bzlreg {
struct add_module_options {
	std::filesystem::path registry_dir;
	std::string_view      main_archive_url;

	std::vector<std::string> supplement_archive_urls;
};

auto add_module(add_module_options options) -> int;
} // namespace bzlreg
