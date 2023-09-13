#pragma once

#include <filesystem>
#include <string_view>

namespace bzlreg {
auto add_module( //
	std::filesystem::path registry_dir,
	std::string_view      archive_url
) -> int;
}
