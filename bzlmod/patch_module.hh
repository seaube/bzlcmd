#pragma once

#include <string_view>

namespace bzlmod {
auto patch_module(std::string_view module_name) -> int;
}
