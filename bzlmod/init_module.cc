#include "bzlmod/init_module.hh"

#include <filesystem>
#include <iostream>
#include <format>
#include <string>
#include <fstream>
#include "absl/strings/ascii.h"

namespace fs = std::filesystem;

constexpr auto DEFAULT_MODULE_BAZEL = R"starlark(
module(
	name = "{}",
	version = "0.1.0",
	compatibility_level = 1,
)
)starlark";

constexpr auto DEFAULT_BAZELRC = R"bazelrc(
common --enable_bzlmod
)bazelrc";

auto bzlmod::init_module( //
	fs::path dir
) -> int {
	auto module_file = dir / "MODULE.bazel";
	auto workspace_file = dir / "WORKSPACE.bazel";
	auto workspace_bzlmod_file = dir / "WORKSPACE.bzlmod";
	auto bazelrc_file = dir / ".bazelrc";

	if(fs::exists(module_file)) {
		std::cerr << std::format(
			"[ERROR] Cannot init module. {} already exists.",
			dir.generic_string()
		);
		return 1;
	}

	{
		auto module_file_stream = std::ofstream{module_file};

		module_file_stream << absl::StripAsciiWhitespace(
			std::format(DEFAULT_MODULE_BAZEL, dir.filename().string())
		);
		module_file_stream << "\n";
	}

	if(!fs::exists(workspace_file) && !fs::exists(workspace_bzlmod_file)) {
		auto workspace_bzlmod_file_stream = std::ofstream{workspace_bzlmod_file};
		workspace_bzlmod_file_stream << "\n";
	}

	if(!fs::exists(bazelrc_file)) {
		auto bazelrc_file_stream = std::ofstream{bazelrc_file};
		bazelrc_file_stream << absl::StripAsciiWhitespace(DEFAULT_BAZELRC);
		bazelrc_file_stream << "\n";
	}

	return 0;
}
