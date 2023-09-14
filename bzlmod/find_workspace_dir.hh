#pragma once

#include <filesystem>
#include <optional>

namespace bzlmod {
  auto find_workspace_dir(
    std::filesystem::path start_dir
  ) -> std::optional<std::filesystem::path>;
}
