#include "bzlmod/find_workspace_dir.hh"

#include <filesystem>

namespace fs = std::filesystem;

auto bzlmod::find_workspace_dir( //
	std::filesystem::path start_dir
) -> std::optional<std::filesystem::path> {
	if(fs::exists(start_dir / "MODULE.bazel")) {
		return start_dir;
	}

	if(start_dir.has_parent_path()) {
		return find_workspace_dir(start_dir.parent_path());
	}

	return std::nullopt;
}
