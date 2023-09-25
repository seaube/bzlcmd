#pragma once

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

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(maintainer_config, email, name)
	};

	std::string                                  homepage;
	std::vector<maintainer_config>               maintainers;
	std::vector<std::string>                     repository;
	std::vector<std::string>                     versions;
	std::unordered_map<std::string, std::string> yanked_versions;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(
		metadata_config,
		homepage,
		maintainers,
		repository,
		versions,
		yanked_versions
	)
};

struct source_config {
	std::string                                  integrity;
	std::string                                  strip_prefix;
	int                                          patch_strip;
	std::unordered_map<std::string, std::string> patches;
	std::string                                  url;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(
		source_config,
		integrity,
		strip_prefix,
		patch_strip,
		patches,
		url
	)
};
} // namespace bzlreg
