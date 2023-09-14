#include <filesystem>
#include <iostream>
#include "docopt.h"
#include "bzlmod/init_module.hh"
#include "bzlmod/add_module.hh"

namespace fs = std::filesystem;

constexpr auto USAGE = R"docopt(
Bzlmod - manage your bazel module with _ease_

Usage:
	bzlmod init [<module-dir>]
	bzlmod add <dep-name>
)docopt";

auto main(int argc, char* argv[]) -> int {
	auto bazel_working_dir = std::getenv("BUILD_WORKING_DIRECTORY");
	if(bazel_working_dir != nullptr) {
		fs::current_path(bazel_working_dir);
	}

	auto args = docopt::docopt(USAGE, {argv + 1, argv + argc});
	auto exit_code = int{0};

	if(args["init"].asBool()) {
		auto module_dir = args["<module-dir>"] //
			? fs::path{args.at("<module-dir>").asString()}
			: fs::current_path();

		exit_code = bzlmod::init_module(module_dir);
	} else if(args["add"].asBool()) {
    auto dep_name = args["<dep-name>"].asString();
		exit_code = bzlmod::add_module(dep_name);
	}

	return exit_code;
}
