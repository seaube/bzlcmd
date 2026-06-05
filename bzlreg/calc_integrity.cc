#include "bzlreg/calc_integrity.hh"

#include <print>
#include <filesystem>
#include <fstream>
#include "nlohmann/json.hpp"
#include "bzlreg/util.hh"
#include "bzlreg/config_types.hh"

using json = nlohmann::json;
namespace fs = std::filesystem;

auto bzlreg::calc_integrity( //
	const calc_integrity_options& options
) -> int {
	if(!fs::exists(options.registry_dir / "bazel_registry.json")) {
		std::println(
			stderr,
			"bazel_registry.json file is missing. Are sure {} is a bazel registry?",
			options.registry_dir.generic_string()
		);
		return 1;
	}

	auto module_dir = options.registry_dir / "modules" / options.module_name;
	auto metadata_config_path = module_dir / "metadata.json";

	if(!fs::exists(metadata_config_path)) {
		std::println( //
			stderr,
			"[ERROR] {} does not exist",
			metadata_config_path.generic_string()
		);
		return 1;
	}

	bzlreg::metadata_config metadata_config =
		json::parse(std::ifstream{metadata_config_path});

	for(auto version : metadata_config.versions) {
		auto source_config_path = module_dir / version / "source.json";
		bzlreg::calc_source_integrity(source_config_path);
	}

	return 0;
}
