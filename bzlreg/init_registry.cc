#include "bzlreg/init_registry.hh"

#include <fstream>
#include "bzlreg/config_types.hh"

namespace fs = std::filesystem;
using nlohmann::json;

auto bzlreg::init_registry( //
	fs::path registry_dir
) -> int {
	auto ec = std::error_code{};
	auto bazel_registry_json_path = registry_dir / "bazel_registry.json";
	auto modules_dir = registry_dir / "modules";

	if(!fs::exists(modules_dir)) {
		fs::create_directories(modules_dir, ec);
	}

	if(!fs::exists(bazel_registry_json_path)) {
		auto config = bzlreg::bazel_registry_config{};
		auto config_json = json{};
		to_json(config_json, config);
		std::ofstream{bazel_registry_json_path} << config_json.dump(4);
	}

	return 0;
}
