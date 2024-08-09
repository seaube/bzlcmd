#include "bzlreg/add_module.hh"

#include <string_view>
#include <filesystem>
#include <iostream>
#include <format>
#include <fstream>
#include <memory>
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
using namespace std::string_view_literals;

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

[[nodiscard]] static auto validate_archive_url( //
	std::string_view archive_url_str
) -> boost::url {
	if(!is_valid_archive_url(archive_url_str)) {
		std::cerr << std::format( //
			"Invalid archive URL {}\nMust begin with https:// or http://\n",
			archive_url_str
		);
		std::exit(1);
	}

	auto archive_url = boost::urls::url{archive_url_str};
	auto archive_filename = fs::path{archive_url.path()}.filename().string();
	if(!archive_filename.ends_with(".tar.gz") && !archive_filename.ends_with(".tgz")) {
		std::cerr << std::format(
			"Archive {} is not supported. Only .tar.gz archives are allowed.\n",
			archive_filename
		);
		std::exit(1);
	}

	return archive_url;
}

auto bzlreg::add_module(add_module_options options) -> int {
	auto registry_dir = options.registry_dir;
	auto main_archive_url_str = options.main_archive_url;

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

	auto main_archive_url = validate_archive_url(main_archive_url_str);
	auto supplement_archive_urls = std::vector<boost::url>{};
	supplement_archive_urls.reserve(options.supplement_archive_urls.size());
	for(auto archive_url_str : options.supplement_archive_urls) {
		supplement_archive_urls.emplace_back(validate_archive_url(archive_url_str));
	}

	struct archive_info {
		std::shared_ptr<std::vector<std::byte>> archive_bytes;

		bzlreg::tar_view tar_view;
		std::string      common_prefix;
		std::string      integrity;
	};

	auto archives = std::vector<archive_info>{};
	archives.reserve(supplement_archive_urls.size() + 1);

	auto download_and_add = [&archives](std::string_view archive_url) {
		auto compressed_data = bzlreg::download_file(archive_url);
		if(!compressed_data) {
			std::cerr << std::format("Failed to download {}\n", archive_url);
			std::exit(1);
		}

		auto integrity = calc_sha256_integrity(*compressed_data);
		if(integrity.empty()) {
			std::cerr << "Failed to calculate sha256 integrity\n";
			std::exit(1);
		}

		auto prefix = std::string{}; // TODO: find prefix
		auto archive_bytes = std::make_shared<std::vector<std::byte>>(
			bzlreg::decompress_archive(*compressed_data)
		);

		archives.emplace_back(
			archive_bytes,
			bzlreg::tar_view(*archive_bytes),
			prefix,
			integrity
		);
	};

	download_and_add(main_archive_url_str);
	for(auto archive_url : supplement_archive_urls) {
		download_and_add(archive_url.c_str());
	}

	auto source_config = bzlreg::source_config{
		.integrity = archives[0].integrity,
		.strip_prefix = archives[0].common_prefix,
		.patch_strip = 0,
		.patches = {},
		.url = std::string{main_archive_url_str},
	};

	auto module_bzl = bzlreg::module_bazel{};
	auto module_bzl_view = bzlreg::tar_view_file{};

	for(auto& archive : std::ranges::reverse_view{archives}) {
		module_bzl_view =
			archive.tar_view.file(archive.common_prefix + "MODULE.bazel");
	}

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

	if(!metadata_config.repository) {
		metadata_config.repository.emplace();
	}
	if(metadata_config.repository->empty()) {
		auto inferred_repository = infer_repository_from_url(main_archive_url);
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
	std::ofstream{module_bazel_path} << module_bzl_view.string_view();

	return 0;
}
