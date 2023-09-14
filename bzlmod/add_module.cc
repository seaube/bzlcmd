#include "bzlmod/add_module.hh"

#include <filesystem>
#include <iostream>
#include <algorithm>
#include <execution>
#include <format>
#include <boost/process.hpp>
#include "bzlmod/get_registries.hh"
#include "bzlmod/find_workspace_dir.hh"
#include "bzlmod/download_module_metadata.hh"

namespace bp = boost::process;
namespace fs = std::filesystem;

struct registry_resolve_entry {
	std::string_view registry;
	std::string      module_version;
};

auto bzlmod::add_module( //
	std::string_view dep_name
) -> int {
	auto buildozer = bp::search_path("buildozer");
	if(buildozer.empty()) {
		std::cerr << std::format(
			"[ERROR] `buildozer` is required to use `bzlmod add`. Please make sure "
			"it's in your PATH. Buildozer may be downloaded here:\n"
			"        https://github.com/bazelbuild/buildtools/releases\n\n"
		);
		return 1;
	}

	auto workspace_dir = find_workspace_dir(fs::current_path());

	if(!workspace_dir) {
		std::cerr << std::format(
			"[ERROR] Cannot find bazel workspace from {}."
			"        Did you mean `bzlmod init`?\n",
			fs::current_path().generic_string()
		);
		return 1;
	}

	auto registries = get_registries(*workspace_dir);

	if(!registries) {
		std::cerr << "[ERROR] Unable to read .bazelrc file(s)\n";
		return 1;
	}

	auto registry_resolve_entries = std::vector<registry_resolve_entry>{};
	registry_resolve_entries.reserve(registries->size());
	for(auto& registry : *registries) {
		registry_resolve_entries.emplace_back(registry, "");
	}

	std::for_each(
		std::execution::par,
		registry_resolve_entries.begin(),
		registry_resolve_entries.end(),
		[&](registry_resolve_entry& entry) {
			auto metadata_url = std::format( //
				"{}/modules/{}/metadata.json",
				entry.registry,
				dep_name
			);
			auto metadata = bzlmod::download_module_metadata(metadata_url);

			if(metadata && !metadata->versions.empty()) {
				entry.module_version = metadata->versions.back();
			}
		}
	);

	auto dep_version = std::optional<std::string>{};

	for(auto& entry : registry_resolve_entries) {
		if(!entry.module_version.empty()) {
			dep_version = entry.module_version;
			break;
		}
	}

	if(!dep_version) {
		std::cerr << "Failed to find " << dep_name << " in:\n";
		for(auto& entry : registry_resolve_entries) {
			std::cerr << "\t" << entry.registry << "\n";
		}
		return 1;
	}

	// We don't care if this fails
	bp::child{
		bp::exe(buildozer),
		bp::args({
			std::format("new bazel_dep {}", dep_name),
			"//MODULE.bazel:all",
		}),
		bp::std_out > bp::null,
		bp::std_err > bp::null,
	}
		.wait();

	auto buildozer_proc = bp::child{
		bp::exe(buildozer),
		bp::args({
			std::format("set version {}", *dep_version),
			std::format("//MODULE.bazel:{}", dep_name),
		}),
		bp::std_out > bp::null,
		bp::std_err > bp::null,
	};

	buildozer_proc.wait();

	auto buildozer_exit_code = buildozer_proc.exit_code();

	if(buildozer_exit_code == 0) {
		std::cout << std::format( //
			"{}@{} added\n",
			dep_name,
			*dep_version
		);
	} else if(buildozer_exit_code == 3) {
		std::cout << std::format( //
			"{}@{} already added\n",
			dep_name,
			*dep_version
		);
	} else {
		std::cerr << std::format( //
			"buildozer exited with {}\n",
			buildozer_proc.exit_code()
		);
		return 1;
	}

	return 0;
}
