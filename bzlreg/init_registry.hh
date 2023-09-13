#pragma once

#include <filesystem>

namespace bzlreg {
auto init_registry(std::filesystem::path registry_dir) -> int;
}
