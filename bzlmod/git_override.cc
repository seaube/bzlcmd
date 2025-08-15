#include "bzlmod/git_override.hh"

#include <string>
#include <string_view>
#include <iostream>
#include <filesystem>
#include <execution>
#include <fstream>
#include <boost/process.hpp>
#include "bzlmod/get_registries.hh"
#include "bzlmod/find_workspace_dir.hh"
#include "bzlmod/download_module_metadata.hh"

namespace bp = boost::process;
namespace fs = std::filesystem;
using namespace std::string_literals;
using namespace std::string_view_literals;

struct registry_resolve_entry {
	std::string              registry;
	std::string              module_version;
	std::vector<std::string> repositories;
};

constexpr auto GIT_OVERRIDE_SNIPPET = R"(
git_override(
    module_name = "{}",
    commit = "{}",
    remote = "{}",
)
)";

auto bzlmod::git_override(const git_override_options& options) -> int {
	auto buildozer = bp::search_path("buildozer");
	if(buildozer.empty()) {
		std::cerr << std::format(
			"[ERROR] `buildozer` is required to use `bzlmod git-override`. Please "
			"make sure "
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
		assert(!registry.empty());
		auto& entry = registry_resolve_entries.emplace_back();
		entry.registry = registry;
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
				options.dep
			);
			auto metadata = bzlmod::download_module_metadata(metadata_url);

			if(metadata && !metadata->versions.empty()) {
				entry.module_version = metadata->versions.back();
			}
			if(metadata && metadata->repository) {
				entry.repositories = *metadata->repository;
			}
		}
	);

	auto found_entry = std::optional<registry_resolve_entry>{};

	for(auto& entry : registry_resolve_entries) {
		if(!entry.module_version.empty()) {
			found_entry = entry;
			break;
		}
	}

	if(!found_entry) {
		std::cerr << "Failed to find " << options.dep << " in:\n";
		for(auto& entry : registry_resolve_entries) {
			std::cerr << "\t" << entry.registry << "\n";
		}
		return 1;
	}

	auto module_file_path = *workspace_dir / "MODULE.bazel";
	auto module_file = std::ofstream{
		module_file_path,
		std::ios_base::binary | std::ios_base::app,
	};

	auto gh_repo = std::string{};
	for(auto repo : found_entry->repositories) {
		if(repo.starts_with("github:")) {
			gh_repo = repo.substr("github:"sv.size());
			break;
		}
	}

	if(gh_repo.empty()) {
		std::cerr << "[ERROR] only github based modules are supported\n";
		return 1;
	}

	auto slash_idx = gh_repo.find("/");
	auto org_name = gh_repo.substr(0, slash_idx);
	auto repo_name = gh_repo.substr(slash_idx + 1);
	auto remote = std::format("git@github.com:{}/{}.git", org_name, repo_name);
	auto commit = std::string{};

	auto gh_exe = bp::search_path("gh");
	auto gh_stdout_stream = bp::ipstream{};
	auto gh_proc = bp::child{
		bp::exe(gh_exe),
		bp::args({
			"api"s,
			std::format(
				"repos/{}/{}/commits/{}",
				org_name,
				repo_name,
				options.committish.value_or("HEAD")
			),
			"--jq"s,
			".sha"s,
		}),
		bp::std_out > gh_stdout_stream,
	};

	std::getline(gh_stdout_stream, commit);
	if(commit.empty()) {
		std::cerr << std::format(
			"[ERROR] failed to get commit for {} \n",
			options.committish.value_or("HEAD")
		);
		return 1;
	}

	module_file << std::format(GIT_OVERRIDE_SNIPPET, options.dep, commit, remote);

	return 0;
}
