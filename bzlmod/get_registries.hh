#pragma once

#include <vector>
#include <string>
#include <optional>
#include <filesystem>

namespace bzlmod {

/**
 * Get configured bazel registries
 * @returns `nullopt` if bazel workspace couldn't be found or bazerc files could
 * not be read
 */
auto get_registries( //
	std::filesystem::path dir
) -> std::optional<std::vector<std::string>>;

} // namespace bzlmod
