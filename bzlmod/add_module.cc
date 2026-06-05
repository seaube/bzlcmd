#include "bzlmod/add_module.hh"

#include <filesystem>
#include <print>
#include <algorithm>
#include <execution>
#define BOOST_PROCESS_VERSION 1
#include <boost/process/v1.hpp>
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
		std::print(
			stderr,
			"[ERROR] `buildozer` is required to use `bzlmod add`. Please make sure "
			"it's in your PATH. Buildozer may be downloaded here:\n"
			"        https://github.com/bazelbuild/buildtools/releases\n\n"
		);
		return 1;
	}

	auto workspace_dir = find_workspace_dir(fs::current_path());

	if(!workspace_dir) {
		std::print(
			stderr,
			"[ERROR] Cannot find bazel workspace from {}."
			"        Did you mean `bzlmod init`?\n",
			fs::current_path().generic_string()
		);
		return 1;
	}

	auto registries = get_registries(*workspace_dir);

	if(!registries) {
		std::println(stderr, "[ERROR] Unable to read .bazelrc file(s)");
		return 1;
	}

	auto registry_resolve_entries = std::vector<registry_resolve_entry>{};
	registry_resolve_entries.reserve(registries->size());
	for(auto& registry : *registries) {
		registry_resolve_entries.emplace_back(registry, "");
	}

	std::for_each(
#ifdef __cpp_lib_parallel_algorithm
		std::execution::par,
#endif
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
		std::println(stderr, "Failed to find {} in:", dep_name);
		for(auto& entry : registry_resolve_entries) {
			std::println(stderr, "\t{}", entry.registry);
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
		std::println( //
			"{}@{} added",
			dep_name,
			*dep_version
		);
	} else if(buildozer_exit_code == 3) {
		std::println( //
			"{}@{} already added",
			dep_name,
			*dep_version
		);
	} else {
		std::println( //
			stderr,
			"buildozer exited with {}",
			buildozer_proc.exit_code()
		);
		return 1;
	}

	return 0;
}
