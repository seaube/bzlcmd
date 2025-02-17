#include <filesystem>
#include <iostream>
#include "docopt.h"
#include "bzlreg/init_registry.hh"
#include "bzlreg/bazel_exec.hh"
#include "bzlreg/add_module.hh"
#include "bzlreg/calc_integrity.hh"

namespace fs = std::filesystem;

constexpr auto USAGE = R"docopt(
Bazel registry CLI utility

Usage:
	bzlreg init [<registry-dir>]
	bzlreg build <label> [--registry=<path>]
	bzlreg test <label> [--registry=<path>]
	bzlreg run <label> [--registry=<path>]
	bzlreg add-module <archive-url> [--strip-prefix=<str>] [--registry=<path>]
	bzlreg calc-integrity <module> [--strip-prefix=<str>] [--registry=<path>]

Options:
	--registry=<path>     Registry directory. Defaults to current working directory.
	--strip-prefix=<str>  Prefix stripped from archive and set in source.json.
)docopt";

static auto forward_bazel_subcommand(
	const docopt::Options& options,
	std::string_view       subcommand
) -> int {
	auto registry_dir = options.at("--registry") //
		? fs::path{options.at("--registry").asString()}
		: fs::current_path();
	auto label = options.at("<label>").asString();
	return bzlreg::bazel_exec({
		.registry_dir = registry_dir,
		.label = label,
		.subcommand = subcommand,
	});
}

static auto calc_integrity_command(const docopt::Options& options) -> int {
	auto registry_dir = options.at("--registry") //
		? fs::path{options.at("--registry").asString()}
		: fs::current_path();
	auto module = options.at("<module>").asString();

	return bzlreg::calc_integrity({
		.registry_dir = registry_dir,
		.module_name = module,
	});
}

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
	} else if(args["build"].asBool()) {
		exit_code = forward_bazel_subcommand(args, "build");
	} else if(args["test"].asBool()) {
		exit_code = forward_bazel_subcommand(args, "test");
	} else if(args["run"].asBool()) {
		exit_code = forward_bazel_subcommand(args, "run");
	} else if(args["calc-integrity"].asBool()) {
		exit_code = calc_integrity_command(args);
	} else if(args["add-module"].asBool()) {
		auto strip_prefix = args["--strip-prefix"] //
			? args.at("--strip-prefix").asString()
			: "";
		auto registry_dir = args["--registry"] //
			? fs::path{args.at("--registry").asString()}
			: fs::current_path();

		if(registry_dir.empty()) {
			std::cerr << "[ERROR] --registry must not be empty\n";
			std::cerr << USAGE;
			return 1;
		}

		auto archive_url = args.at("<archive-url>").asString();
		exit_code = bzlreg::add_module({
			.registry_dir = registry_dir,
			.archive_url = archive_url,
			.strip_prefix = strip_prefix,
		});
	}

	return exit_code;
}
