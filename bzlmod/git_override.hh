#pragma once

#include <string>
#include <optional>

namespace bzlmod {
struct git_override_options {
	std::string                dep;
	std::optional<std::string> committish;
};

auto git_override(const git_override_options& options) -> int;
} // namespace bzlmod
