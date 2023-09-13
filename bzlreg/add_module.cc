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

namespace fs = std::filesystem;
using bzlreg::util::defer;
using json = nlohmann::json;

static auto is_valid_archive_url(std::string_view url) -> bool {
	return url.starts_with("https://") || url.starts_with("http://");
}

static auto calc_sha256_integrity(auto&& data) -> std::string {
	auto ctx = EVP_MD_CTX_new();

	if(!ctx) {
		return "";
	}

	auto _ctx_cleanup = defer([&] { EVP_MD_CTX_cleanup(ctx); });

	if(!EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr)) {
		return "";
	}

	if(!EVP_DigestUpdate(ctx, data.data(), data.size())) {
		return "";
	}

	uint8_t      hash[EVP_MAX_MD_SIZE];
	unsigned int hash_length = 0;

	if(!EVP_DigestFinal_ex(ctx, hash, &hash_length)) {
		return "";
	}

	auto b64_str = std::string{};
	b64_str.resize(hash_length * 4);

	auto b64_encode_size = EVP_EncodeBlock(
		reinterpret_cast<uint8_t*>(b64_str.data()),
		hash,
		hash_length
	);
	b64_str.resize(b64_encode_size);

	return "sha256-" + b64_str;
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

auto bzlreg::add_module(add_module_options options) -> int {
	auto registry_dir = options.registry_dir;
	auto archive_url_str = options.archive_url;
	auto strip_prefix = options.strip_prefix;

	if(!fs::exists(registry_dir / "bazel_registry.json")) {
		std::cerr << std::format(
			"bazel_registry.json file is missing. Are sure {} is a bazel registry?",
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
	if(!archive_filename.ends_with(".tar.gz") && !archive_filename.ends_with(".tgz")) {
		std::cerr << std::format(
			"Archive {} is not supported. Only .tar.gz archives are allowed.\n",
			archive_filename
		);
		return 1;
	}

	auto compressed_data = bzlreg::download_archive(archive_url_str);

	auto integrity = calc_sha256_integrity(compressed_data);
	if(integrity.empty()) {
		std::cerr << "Failed to calculate sha256 integrity\n";
		return 1;
	}

	auto decompressed_data = bzlreg::decompress_archive(compressed_data);

	auto tar_view = bzlreg::tar_view{decompressed_data};
	auto module_bzl_view = tar_view.file(
		strip_prefix.empty() //
			? "MODULE.bazel"
			: std::string{strip_prefix} + "/MODULE.bazel"
	);
	if(!module_bzl_view) {
		std::cerr << "Failed to find MODULE.bazel in archive\n";
		return 1;
	}

	auto module_bzl = bzlreg::module_bazel::parse(module_bzl_view.string_view());

	if(!module_bzl) {
		std::cerr << "Failed to parse MODULE.bazel\n";
		return 1;
	}

	auto source_config = bzlreg::source_config{
		.integrity = integrity,
		.strip_prefix = std::string{strip_prefix},
		.patch_strip = 0,
		.patches = {},
		.url = std::string{archive_url_str},
	};

	auto module_dir = modules_dir / module_bzl->name;
	auto source_config_path = module_dir / module_bzl->version / "source.json";
	auto module_bazel_path = module_dir / module_bzl->version / "MODULE.bazel";
	fs::create_directories(source_config_path.parent_path(), ec);

	auto metadata_config_path = module_dir / "metadata.json";
	auto metadata_config = bzlreg::metadata_config{};

	if(fs::exists(metadata_config_path)) {
		metadata_config = json::parse(std::ifstream{metadata_config_path});
	}

	for(auto version : metadata_config.versions) {
		if(module_bzl->version == version) {
			std::cerr << std::format( //
				"{} already exists for {}\n",
				version,
				module_bzl->name
			);
			return 1;
		}
	}

	metadata_config.versions.emplace_back(module_bzl->version);

	if(metadata_config.repository.empty()) {
		auto inferred_repository = infer_repository_from_url(archive_url);
		if(inferred_repository) {
			metadata_config.repository.emplace_back(*inferred_repository);
		} else {
			std::cerr << std::format( //
				"[WARN] Unable to infer repository string from {}\n"
				"       Please add to {} manually",
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
	std::ofstream{module_bazel_path} << module_bzl_view.string_view();

	return 0;
}
