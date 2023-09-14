#pragma once

#include <string_view>
#include <optional>
#include "bzlreg/config_types.hh"

namespace bzlmod {
auto download_module_metadata( //
	std::string_view url
) -> std::optional<bzlreg::metadata_config>;
}