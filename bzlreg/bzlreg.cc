#include <filesystem>
#include <fstream>
#include <string>
#include <format>
#include <iostream>
#include "nlohmann/json.hpp"
#include "docopt.h"
#include "bzlreg/config_types.hh"

namespace fs = std::filesystem;
using nlohmann::json;

constexpr auto USAGE = R"docopt(
Bazel registry CLI utility

Usage:
  bzlreg init [<registry-dir>]
	bzlreg add-module <archive-url> [--registry-dir=<registry-dir>]
)docopt";

auto init_registry(fs::path registry_dir) -> int {
	auto bazel_registry_json_path = registry_dir / "bazel_registry.json";
	auto modules_dir = registry_dir / "modules";

	if(!fs::exists(bazel_registry_json_path)) {
		auto config = bzlreg::bazel_registry_config{};
		auto config_json = json{};
		to_json(config_json, config);
		std::ofstream{bazel_registry_json_path} << config_json.dump(4);
	}

	if(!fs::exists(modules_dir)) {
		fs::create_directory(modules_dir);
	}

	return 0;
}

auto init_module( //
	fs::path    registry_dir,
	std::string module_name
) -> int {
	return 0;
}

auto is_valid_archive_url(const std::string& url) {
	return url.starts_with("https://") || url.starts_with("http://");
}

auto add_module( //
	fs::path    registry_dir,
	std::string archive_url
) -> int {
	if(!is_valid_archive_url(archive_url)) {
		std::cerr << std::format( //
			"Invalid archive URL {}\nMust begin with https:// or http://\n",
			archive_url
		);
		return 1;
	}

	return 0;
}

auto main(int argc, char* argv[]) -> int {
	auto args = docopt::docopt(USAGE, {argv + 1, argv + argc});

	auto exit_code = int{0};

	if(args["init"].asBool()) {
		auto registry_dir = args["<registry-dir>"] //
			? fs::path{args.at("<registry-dir>").asString()}
			: fs::current_path();

		exit_code = init_registry(registry_dir);
	} else if(args["add-module"].asBool()) {
		auto registry_dir = args["--registry_dir"] //
			? fs::path{args.at("--registry_dir").asString()}
			: fs::current_path();
		auto archive_url = args.at("<archive-url>").asString();
		exit_code = add_module(registry_dir, archive_url);
	}

	return exit_code;
}
