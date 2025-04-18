#include "bzlreg/add_module.hh"

#include <string_view>
#include <filesystem>
#include <iostream>
#include <format>
#include <fstream>
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

namespace fs = std::filesystem;
using bzlreg::util::defer;
using json = nlohmann::json;

static auto is_valid_archive_url(std::string_view url) -> bool {
	return url.starts_with("https://") || url.starts_with("http://");
}

auto infer_repository_from_url( //
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

static auto infer_module_version( //
	bzlreg::tar_view tar_view,
	std::string_view strip_prefix
) -> std::string {
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

		if(name == "pax_global_header") {
			continue;
		}

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

	auto archive_url = boost::urls::url{archive_url_str};
	auto archive_filename = fs::path{archive_url.path()}.filename().string();
	if(!archive_filename.ends_with(".tar.gz") &&
		 !archive_filename.ends_with(".tgz")) {
		std::cerr << std::format(
			"Archive {} is not supported. Only .tar.gz archives are allowed.\n",
			archive_filename
		);
		return 1;
	}

	auto compressed_data = bzlreg::download_file(archive_url_str);
	if(!compressed_data) {
		std::cerr << std::format("Failed to download {}\n", archive_url_str);
		return 1;
	}

	auto integrity = bzlreg::calc_integrity(
		std::as_bytes(std::span{compressed_data->data(), compressed_data->size()})
	);
	if(!integrity) {
		std::cerr << "Failed to calculate integrity\n";
		return 1;
	}

	auto decompressed_data = bzlreg::decompress_archive(*compressed_data);
	if(decompressed_data.empty()) {
		std::cerr << "Failed to decompress archive data\n";
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
		std::cerr << "[WARN] no MODULE.bazel file found in archive\n";
		module_name = infer_module_name(tar_view, strip_prefix);
		module_version = infer_module_version(tar_view, strip_prefix);
	} else {
		module_bzl = bzlreg::module_bazel::parse(module_bzl_view.string_view());

		if(!module_bzl) {
			std::cerr << "Failed to parse MODULE.bazel\n";
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
				"{} already exists for {}\n",
				version,
				module_name
			);
			return 1;
		}
	}

	metadata_config.versions.emplace_back(module_version);

	if(!metadata_config.repository) {
		metadata_config.repository.emplace();
	}
	if(metadata_config.repository->empty()) {
		auto inferred_repository = infer_repository_from_url(archive_url);
		if(inferred_repository) {
			metadata_config.repository->emplace_back(*inferred_repository);
		} else {
			std::cerr << std::format( //
				"[WARN] Unable to infer repository string from {}\n"
				"       Please add to {} manually\n",
				archive_url_str,
				metadata_config_path.generic_string()
			);
		}
	}

	if(metadata_config.maintainers.empty()) {
		std::cerr << std::format( //
			"[WARN] 'maintainers' list is empty in {}\n",
			metadata_config_path.generic_string()
		);
	}

	if(metadata_config.homepage.empty()) {
		std::cerr << std::format( //
			"[WARN] 'homepage' is empty in {}\n",
			metadata_config_path.generic_string()
		);
	}

	std::ofstream{metadata_config_path} << json{metadata_config}[0].dump(4);
	std::ofstream{source_config_path} << json{source_config}[0].dump(4);
	if(module_bzl_view) {
		std::ofstream{module_bazel_path} << module_bzl_view.string_view();
	} else {
	}

	return 0;
}
