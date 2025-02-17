#pragma once

#include <filesystem>

namespace bzlreg {
struct bazel_exec_options {
	std::filesystem::path registry_dir;
	std::string_view      label;
	std::string_view      subcommand;
};

auto bazel_exec(const bazel_exec_options& options) -> int;
} // namespace bzlreg
