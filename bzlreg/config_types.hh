#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "nlohmann/json.hpp"

namespace bzlreg {
struct bazel_registry_config {
	std::vector<std::string> mirrors;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(bazel_registry_config, mirrors);
};

struct metadata_config {
	struct maintainer_config {
		std::string email;
		std::string name;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(maintainer_config, email, name)
	};

	std::string                                  homepage;
	std::vector<maintainer_config>               maintainers;
	std::optional<std::vector<std::string>>      repository;
	std::vector<std::string>                     versions;
	std::unordered_map<std::string, std::string> yanked_versions;
};

inline void to_json(nlohmann::json& j, const metadata_config& metadata) {
	j = nlohmann::json{
		{"homepage", metadata.homepage},
		{"maintainers", metadata.maintainers},
		{"versions", metadata.versions},
		{"yanked_versions", metadata.yanked_versions},
	};

	if(metadata.repository) {
		j["repository"] = *metadata.repository;
	}
}

inline void from_json(const nlohmann::json& j, metadata_config& metadata) {
	j.at("homepage").get_to(metadata.homepage);
	j.at("maintainers").get_to(metadata.maintainers);
	j.at("versions").get_to(metadata.versions);
	j.at("yanked_versions").get_to(metadata.yanked_versions);

	if(j.contains("repository")) {
		if(!metadata.repository) {
			metadata.repository.emplace();
		}
		j.at("repository").get_to(*metadata.repository);
	} else {
		metadata.repository = std::nullopt;
	}
}

struct source_config {
	std::string                                  integrity;
	std::string                                  strip_prefix;
	int                                          patch_strip = 0;
	std::unordered_map<std::string, std::string> patches;
	std::unordered_map<std::string, std::string> overlay;
	std::string                                  url;
};

inline void to_json(nlohmann::json& j, const source_config& source) {
	j = nlohmann::json{
		{"integrity", source.integrity},
		{"strip_prefix", source.strip_prefix},
		{"url", source.url},
	};
	if(!source.patches.empty()) {
		j["patches"] = source.patches;
		j["patch_strip"] = source.patch_strip;
	}
	if(!source.overlay.empty()) {
		j["overlay"] = source.overlay;
	}
}

inline void from_json(const nlohmann::json& j, source_config& source) {
	j.at("integrity").get_to(source.integrity);
	j.at("strip_prefix").get_to(source.strip_prefix);
	j.at("url").get_to(source.url);

	if(j.contains("patches")) {
		j.at("patches").get_to(source.patches);
	} else {
		source.patches.clear();
	}

	if(j.contains("patch_strip")) {
		j.at("patch_strip").get_to(source.patch_strip);
	} else {
		source.patch_strip = 0;
	}

	if(j.contains("overlay")) {
		j.at("overlay").get_to(source.overlay);
	} else {
		source.overlay.clear();
	}
}
} // namespace bzlreg
