#include "bzlmod/download_module_metadata.hh"

#include "nlohmann/json.hpp"
#include "bzlreg/download.hh"

using nlohmann::json;

auto bzlmod::download_module_metadata( //
	std::string_view url
) -> std::optional<bzlreg::metadata_config> {
	auto data = bzlreg::download_file(url);
	if(!data) {
		return std::nullopt;
	}

	bzlreg::metadata_config metadata = json::parse(*data);
	return metadata;
}
