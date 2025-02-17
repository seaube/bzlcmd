#include "bzlreg/bazel_exec.hh"

#include <iostream>
#include <filesystem>
#include <fstream>
#include <boost/process.hpp>
#include "nlohmann/json.hpp"
#include "bzlreg/defer.hh"
#include "bzlreg/unused.hh"
#include "bzlreg/calc_integrity.hh"
#include "bzlreg/config_types.hh"

namespace bp = boost::process;
namespace fs = std::filesystem;
using json = nlohmann::json;

static auto get_module_from_label(std::string_view label) -> std::string_view {
	label = label.substr(1, std::string::npos); // strip '@'
	label = label.substr(0, label.find('/'));
	return label;
}

auto bzlreg::bazel_exec(const bazel_exec_options& options) -> int {
	if(!options.label.starts_with('@')) {
		std::cerr << "[ERROR] label must start with @\n";
		return 1;
	}

	auto module_name = get_module_from_label(options.label);
	auto temp_dir = fs::temp_directory_path() / "bzlreg-exec" / module_name;
	auto ec = std::error_code{};
	auto exit_code = 0;

	if(!fs::exists(options.registry_dir / "bazel_registry.json")) {
		std::cerr << std::format(
			"bazel_registry.json file is missing. Are sure {} is a bazel registry?\n",
			options.registry_dir.generic_string()
		);
		return 1;
	}

	auto module_dir = options.registry_dir / "modules" / module_name;
	auto metadata_config_path = module_dir / "metadata.json";

	if(!fs::exists(metadata_config_path)) {
		std::cerr << std::format( //
			"[ERROR] {} does not exist\n",
			metadata_config_path.generic_string()
		);
		return 1;
	}

	bzlreg::metadata_config metadata_config =
		json::parse(std::ifstream{metadata_config_path});

	auto module_version =
		metadata_config.versions.at(metadata_config.versions.size() - 1);

	exit_code = bzlreg::calc_integrity({
		.registry_dir = options.registry_dir,
		.module_name = std::string{module_name},
	});

	if(exit_code != 0) {
		return exit_code;
	}

	fs::create_directories(temp_dir, ec);
	UNUSED(auto) =
		bzlreg::util::defer([=]() mutable { fs::remove_all(temp_dir, ec); });

	{
		auto module_file =
			std::ofstream{temp_dir / "MODULE.bazel", std::ios_base::binary};

		module_file << std::format( //
			"bazel_dep(name = \"{}\", version = \"{}\")\n",
			module_name,
			module_version
		);

		auto bazelrc_file = std::ofstream{temp_dir / ".bazelrc"};
		auto local_registry_bazelrc_path =
			fs::absolute(options.registry_dir).generic_string();
#ifdef _WIN32
		// strip off drive letter e.g. C:
		local_registry_bazelrc_path = local_registry_bazelrc_path.substr(2);
#endif

		bazelrc_file << "common --enable_bzlmod\n";
		bazelrc_file << std::format(
			"common --registry=file://{}\n",
			local_registry_bazelrc_path
		);
		bazelrc_file << "common --registry=https://bcr.bazel.build\n";
	}

	auto bazel_exe = bp::search_path("bazel");
	auto bazel_proc = bp::child{
		bp::exe(bazel_exe),
		bp::start_dir(temp_dir.generic_string()),
		bp::args({
			std::string{options.subcommand},
			std::string{options.label},
		}),
		// bp::std_out > stdout,
		// bp::std_err > stderr,
		// bp::std_in < stdin,
	};

	bazel_proc.wait();

	return bazel_proc.exit_code();
}
