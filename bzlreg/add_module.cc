#include "bzlreg/add_module.hh"

#include <string_view>
#include <filesystem>
#include <iostream>
#include <format>
#include <fstream>
#include <chrono>
#include <boost/url.hpp>
#include <openssl/evp.h>
#include "nlohmann/json.hpp"
#include "absl/strings/str_split.h"
#include "bzlreg/download.hh"
#include "bzlreg/decompress.hh"
#include "bzlreg/tar_view.hh"
#include "bzlreg/defer.hh"
#include "bzlreg/config_types.hh"
#include "bzlreg/module_bazel.hh"
#include "bzlreg/util.hh"
#include "bzlreg/gh_exec.hh"

namespace fs = std::filesystem;
using bzlreg::util::defer;
using json = nlohmann::json;

constexpr auto DEFAULT_MODULE_BAZEL = R"starlark(module(
    name = "{}",
    version = "{}",
)
)starlark";

constexpr auto GIT_COMMIT_DEFALT_MODULE_BAZEL = R"starlark(module(
    name = "{}",
    # version determined by git commit author date
    # commit: {}
    version = "{}",
)
)starlark";

struct github_url_info {
	std::string org = {};
	std::string repo = {};
	std::string default_branch = {};
	std::string default_branch_commit = {};
};

struct resolve_archive_url_result {
	boost::urls::url url = {};
	github_url_info  github = {};
};

static auto is_valid_archive_url(std::string_view url) -> bool {
	return url.starts_with("https://") || url.starts_with("http://");
}

static auto infer_repository_from_url( //
	boost::url url
) -> std::optional<std::string> {
	if(url.host_name() == "github.com") {
		std::vector<std::string> path_parts =
			absl::StrSplit(url.path(), absl::MaxSplits('/', 3));
		if(path_parts.size() >= 2) {
			return std::format("github:{}/{}", path_parts[0], path_parts[1]);
		}
	}

	return std::nullopt;
}

static auto infer_module_name( //
	bzlreg::tar_view tar_view,
	std::string_view strip_prefix
) -> std::string {
	if(!strip_prefix.empty()) {
		return std::string{strip_prefix.substr(0, strip_prefix.find('-'))};
	}

	// TODO: search for some common files
	return "";
}

static auto commit_date_to_version_string(std::string commit_date)
	-> std::string {
	if(!commit_date.empty() && commit_date.back() == 'Z') {
		commit_date.pop_back();
	}

	auto iss = std::istringstream{commit_date};
	auto tp = std::chrono::system_clock::time_point{};
	iss >> std::chrono::parse("%FT%T", tp);
	if(iss.fail()) {
		std::cerr << "ERROR: failed to stringify parsed chrono date\n";
		std::exit(1);
	}

	return std::format("{:%Y%m%d}.0", tp);
}

static auto infer_module_version( //
	const resolve_archive_url_result& archive_url_result,
	bzlreg::tar_view tar_view,
	std::string_view strip_prefix
) -> std::string {
	if(!archive_url_result.github.default_branch_commit.empty()) {
		auto commit_date = bzlreg::gh_commit_date(
			archive_url_result.github.org,
			archive_url_result.github.repo,
			archive_url_result.github.default_branch_commit
		);

		if(!commit_date) {
			std::cerr << "ERROR: failed to get commit date\n";
			std::exit(1);
		}

		return commit_date_to_version_string(*commit_date);
	}

	if(!strip_prefix.empty()) {
		auto dash_idx = strip_prefix.find_last_of('-');
		if(dash_idx != std::string::npos) {
			return std::string{strip_prefix.substr(dash_idx + 1)};
		}
	}

	// TODO: search for some common files
	return "";
}

static auto guess_strip_prefix(bzlreg::tar_view tar_view) -> std::string {
	auto strip_prefix = std::string{};
	for(bzlreg::tar_view_file f : tar_view) {
		auto name = f.name();

		auto slash_idx = name.find("/");
		if(slash_idx == std::string::npos) {
			return "";
		}
		auto prefix = name.substr(0, slash_idx);
		if(strip_prefix.empty()) {
			strip_prefix = prefix;
			continue;
		}

		if(strip_prefix != prefix) {
			return "";
		}
	}

	return strip_prefix;
}

static auto resolve_archive_url(std::string_view url_str)
	-> resolve_archive_url_result {
	auto result = resolve_archive_url_result{};

	result.url = boost::urls::url{url_str};

	if(result.url.host_name() == "github.com") {
		auto path = fs::path{result.url.path().substr(1)};
		auto path_segment_count = std::distance(path.begin(), path.end());

		if(path_segment_count == 2) {
			result.github.org = path.begin()->string();
			result.github.repo = std::next(path.begin())->string();

			if(!bzlreg::is_gh_available()) {
				std::cerr << "ERROR: need 'gh' in PATH to get github info - otherwise "
										 "give full archive url\n";
				std::exit(1);
			}

			auto default_branch =
				bzlreg::gh_default_branch(result.github.org, result.github.repo);
			if(!default_branch) {
				std::cerr << std::format(
					"ERROR: failed to get default branch from github result.github.repo "
					"{}/{}\n",
					result.github.org,
					result.github.repo
				);
				std::exit(1);
			}

			result.github.default_branch = *default_branch;

			auto head_commit = bzlreg::gh_branch_commit_sha(
				result.github.org,
				result.github.repo,
				*default_branch
			);
			if(!head_commit) {
				std::cerr << std::format(
					"ERROR: failed to commit sha for branch '{}' in github "
					"result.github.repo {}/{}\n",
					*default_branch,
					result.github.org,
					result.github.repo
				);
				std::exit(1);
			}

			result.github.default_branch_commit = *head_commit;

			result.url = boost::urls::url{std::format(
				"https://github.com/{}/{}/archive/{}.tar.gz",
				result.github.org,
				result.github.repo,
				*head_commit
			)};
		}
	}

	return result;
}

auto bzlreg::add_module(add_module_options options) -> int {
	auto registry_dir = options.registry_dir;
	auto archive_url_str = options.archive_url;
	auto strip_prefix = std::string{options.strip_prefix};

	if(!fs::exists(registry_dir / "bazel_registry.json")) {
		std::cerr << std::format(
			"bazel_registry.json file is missing. Are sure {} is a bazel registry?\n",
			registry_dir.generic_string()
		);
		return 1;
	}

	auto ec = std::error_code{};
	auto modules_dir = registry_dir / "modules";
	fs::create_directories(modules_dir, ec);

	if(!is_valid_archive_url(archive_url_str)) {
		std::cerr << std::format( //
			"Invalid archive URL {}\nMust begin with https:// or http://\n",
			archive_url_str
		);
		return 1;
	}

	auto archive_url_result = resolve_archive_url(archive_url_str);
	auto archive_filename =
		fs::path{archive_url_result.url.path()}.filename().string();
	if(!archive_filename.ends_with(".tar.gz") &&
		 !archive_filename.ends_with(".tgz")) {
		std::cerr << std::format(
			"Archive {} is not supported. Only .tar.gz archives are allowed.\n",
			archive_filename
		);
		return 1;
	}

	archive_url_str = archive_url_result.url.c_str();

	auto compressed_data = bzlreg::download_file(archive_url_str);
	if(!compressed_data) {
		std::cerr << std::format("ERROR: failed to download {}\n", archive_url_str);
		return 1;
	}

	auto integrity = bzlreg::calc_integrity(
		std::as_bytes(std::span{compressed_data->data(), compressed_data->size()})
	);
	if(!integrity) {
		std::cerr << "ERROR: failed to calculate integrity\n";
		return 1;
	}

	auto decompressed_data = bzlreg::decompress_archive(*compressed_data);
	if(decompressed_data.empty()) {
		std::cerr << "ERROR: failed to decompress archive data\n";
		return 1;
	}

	auto module_name = std::string{};
	auto module_version = std::string{};
	auto module_bzl = std::optional<bzlreg::module_bazel>{};
	auto tar_view = bzlreg::tar_view{decompressed_data};

	if(strip_prefix.empty()) {
		strip_prefix = guess_strip_prefix(tar_view);
	}

	auto module_bzl_view = tar_view.file(
		strip_prefix.empty() //
			? "MODULE.bazel"
			: std::string{strip_prefix} + "/MODULE.bazel"
	);
	if(!module_bzl_view) {
		module_name = infer_module_name(tar_view, strip_prefix);
		module_version =
			infer_module_version(archive_url_result, tar_view, strip_prefix);
		std::cerr << "WARN: no MODULE.bazel file found in archive\n";
		std::cerr << std::format(
			"WARN: inferred module name and version is {}@{}\n",
			module_name,
			module_version
		);
	} else {
		module_bzl = bzlreg::module_bazel::parse(module_bzl_view.string_view());

		if(!module_bzl) {
			std::cerr << "ERROR: failed to parse MODULE.bazel\n";
			return 1;
		}

		module_name = module_bzl->name;
		module_version = module_bzl->version;
	}

	auto source_config = bzlreg::source_config{
		.integrity = *integrity,
		.strip_prefix = std::string{strip_prefix},
		.patch_strip = 0,
		.patches = {},
		.url = std::string{archive_url_str},
	};

	if(module_name.empty() || module_version.empty()) {
		std::cerr << "Couldn't decide on module name or version\n";
		return 1;
	}

	auto module_dir = modules_dir / module_name;
	auto source_config_path = module_dir / module_version / "source.json";
	auto module_bazel_path = module_dir / module_version / "MODULE.bazel";
	fs::create_directories(source_config_path.parent_path(), ec);

	auto metadata_config_path = module_dir / "metadata.json";
	auto metadata_config = bzlreg::metadata_config{};

	if(fs::exists(metadata_config_path)) {
		metadata_config = json::parse(std::ifstream{metadata_config_path});
	}

	for(auto version : metadata_config.versions) {
		if(module_version == version) {
			std::cerr << std::format( //
				"ERROR: {}@{} already exists\n",
				module_name,
				version
			);
			return 1;
		}
	}

	metadata_config.versions.emplace_back(module_version);

	if(!metadata_config.repository) {
		metadata_config.repository.emplace();
	}
	if(metadata_config.repository->empty()) {
		auto inferred_repository =
			infer_repository_from_url(archive_url_result.url);
		if(inferred_repository) {
			metadata_config.repository->emplace_back(*inferred_repository);
		} else {
			std::cerr << std::format( //
				"WARN: Unable to infer repository string from {}\n"
				"      Please add to {} manually\n",
				archive_url_str,
				metadata_config_path.generic_string()
			);
		}
	}

	if(metadata_config.maintainers.empty()) {
		std::cerr << std::format( //
			"WARN: 'maintainers' list is empty in {}\n",
			metadata_config_path.generic_string()
		);
	}

	if(metadata_config.homepage.empty()) {
		std::cerr << std::format( //
			"WARN: 'homepage' is empty in {}\n",
			metadata_config_path.generic_string()
		);
	}

	std::ofstream{metadata_config_path} << json{metadata_config}[0].dump(4);
	std::ofstream{source_config_path} << json{source_config}[0].dump(4);
	if(module_bzl_view) {
		std::ofstream{module_bazel_path} << module_bzl_view.string_view();
	} else if(!archive_url_result.github.default_branch_commit.empty()) {
		std::ofstream{module_bazel_path} << std::format(
			GIT_COMMIT_DEFALT_MODULE_BAZEL,
			module_name,
			archive_url_result.github.default_branch_commit,
			module_version
		);
	} else {
		std::ofstream{module_bazel_path}
			<< std::format(DEFAULT_MODULE_BAZEL, module_name, module_version);
	}

	return 0;
}
