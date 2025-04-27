#include "bzlreg/util.hh"

#include <iostream>
#include <execution>
#include <format>
#include <unordered_map>
#include <string>
#include <fstream>
#include <openssl/evp.h>
#include "bzlreg/defer.hh"
#include "bzlreg/config_types.hh"
#include "bzlreg/unused.hh"
#include "nlohmann/json.hpp"

using bzlreg::util::defer;
using namespace std::string_literals;
using namespace std::string_view_literals;
namespace fs = std::filesystem;
using json = nlohmann::json;

auto bzlreg::calc_integrity( //
	std::span<const std::byte> data
) -> std::optional<std::string> {
	auto* ctx = EVP_MD_CTX_new();

	if(!ctx) {
		return std::nullopt;
	}

	UNUSED(auto) = defer([ctx] {
		// TODO: figure out why this crashes
		// EVP_MD_CTX_free(ctx);
	});

	if(!EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr)) {
		return std::nullopt;
	}

	if(!EVP_DigestUpdate(ctx, data.data(), data.size())) {
		return std::nullopt;
	}

	uint8_t      hash[EVP_MAX_MD_SIZE];
	unsigned int hash_length = 0;

	if(!EVP_DigestFinal_ex(ctx, hash, &hash_length)) {
		return std::nullopt;
	}

	auto b64_str = std::string{};
	b64_str.resize(hash_length * 4);

	auto b64_encode_size = EVP_EncodeBlock(
		reinterpret_cast<uint8_t*>(b64_str.data()),
		hash,
		hash_length
	);
	b64_str.resize(b64_encode_size);

	return std::format("sha256-{}", b64_str);
}

auto bzlreg::calc_source_integrity( //
	std::filesystem::path source_json_path
) -> void {
	bzlreg::source_config source = json::parse(std::ifstream{source_json_path});

	auto module_dir = source_json_path.parent_path();
	auto patches_dir = module_dir / "patches";
	auto overlay_dir = module_dir / "overlay";
	auto integrity_paths = std::unordered_map<std::string, std::string>{};
	auto integrity_errors = std::unordered_map<std::string, std::string>{};

	if(fs::exists(patches_dir)) {
		for(auto& entry : fs::recursive_directory_iterator(patches_dir)) {
			if(entry.is_regular_file()) {
				auto rel_path =
					fs::proximate(entry.path(), module_dir).generic_string();
				integrity_paths.emplace(rel_path, ""s);
				integrity_errors.emplace(rel_path, ""s);
			}
		}
	}

	if(fs::exists(overlay_dir)) {
		for(auto& entry : fs::recursive_directory_iterator(overlay_dir)) {
			if(entry.is_regular_file()) {
				auto rel_path =
					fs::proximate(entry.path(), module_dir).generic_string();
				integrity_paths.emplace(rel_path, ""s);
				integrity_errors.emplace(rel_path, ""s);
			}
		}
	}

	for(auto&& [rel_path, _] : source.patches) {
		if(rel_path.starts_with("..")) {
			integrity_paths.emplace(rel_path, ""s);
			integrity_errors.emplace(rel_path, ""s);
		}
	}

	for(auto&& [rel_path, _] : source.overlay) {
		if(rel_path.starts_with("..")) {
			integrity_paths.emplace(rel_path, ""s);
			integrity_errors.emplace(rel_path, ""s);
		}
	}

	std::for_each(
		std::execution::seq,
		integrity_paths.begin(),
		integrity_paths.end(),
		[&](std::pair<const std::string, std::string>& pair) {
			auto file_path = module_dir / pair.first;
			auto file_content = std::vector<std::byte>{};
			auto ec = std::error_code{};

			read_file_contents(file_path, file_content, ec);

			if(ec) {
				integrity_errors.at(pair.first) = ec.message();
				return;
			}

			auto integrity = calc_integrity(std::span{
				file_content.data(),
				file_content.size(),
			});
			if(!integrity) {
				integrity_errors.at(pair.first) = "calc_integrity() failed"s;
				return;
			}
			pair.second = *integrity;
		}
	);

	auto has_error = false;
	for(auto&& [p, err] : integrity_errors) {
		if(!err.empty()) {
			has_error = true;
			std::cerr << std::format("[ERROR] {}: {}\n", p, err);
		}
	}

	if(has_error) {
		return;
	}

	source.overlay.clear();
	source.patches.clear();

	for(auto&& [p, integrity] : integrity_paths) {
		if(auto prefix = "overlay/"sv; p.starts_with(prefix)) {
			source.overlay.emplace(p.substr(prefix.size()), integrity);
		} else if(auto prefix = "patches/"sv; p.starts_with(prefix)) {
			source.patches.emplace(p.substr(prefix.size()), integrity);
		}
	}

	std::cout << std::format("updating {}\n", source_json_path.generic_string());
	std::ofstream{source_json_path, std::ios_base::binary}
		<< json{source}[0].dump(4, ' ', false);
}
