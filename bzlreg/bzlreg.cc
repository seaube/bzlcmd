#include <filesystem>
#include <iostream>
#include "docopt.h"
#include "bzlreg/init_registry.hh"
#include "bzlreg/add_module.hh"

namespace fs = std::filesystem;

constexpr auto USAGE = R"docopt(
Bazel registry CLI utility

Usage:
	bzlreg init [<registry-dir>]
	bzlreg add-module <archive-url> [--strip-prefix=<strip-prefix>] [--registry-dir=<registry-dir>]
)docopt";

auto main(int argc, char* argv[]) -> int {
	auto bazel_working_dir = std::getenv("BUILD_WORKING_DIRECTORY");
	if(bazel_working_dir != nullptr) {
		fs::current_path(bazel_working_dir);
	}

	auto args = docopt::docopt(USAGE, {argv + 1, argv + argc});
	auto exit_code = int{0};

	if(args["init"].asBool()) {
		auto registry_dir = args["<registry-dir>"] //
			? fs::path{args.at("<registry-dir>").asString()}
			: fs::current_path();

		exit_code = bzlreg::init_registry(registry_dir);
	} else if(args["add-module"].asBool()) {
		auto strip_prefix = args["--strip-prefix"] //
			? args.at("--strip-prefix").asString()
			: "";
		auto registry_dir = args["--registry-dir"] //
			? fs::path{args.at("--registry-dir").asString()}
			: fs::current_path();
		auto archive_url = args.at("<archive-url>").asString();
		exit_code = bzlreg::add_module({
			.registry_dir = registry_dir,
			.archive_url = archive_url,
			.strip_prefix = strip_prefix,
		});
	}

	return exit_code;
}
