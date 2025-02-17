#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "nlohmann/json.hpp"

namespace bzlreg {
struct bazel_registry_config {
	std::vector<std::string> mirrors;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(bazel_registry_config, mirrors);
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
	int                                          patch_strip;
	std::unordered_map<std::string, std::string> patches;
	std::unordered_map<std::string, std::string> overlay;
	std::string                                  url;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(
		source_config,
		integrity,
		strip_prefix,
		patch_strip,
		patches,
		overlay,
		url
	)
};
} // namespace bzlreg
