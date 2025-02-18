#pragma once

#include <optional>
#include <vector>
#include <string>

namespace bzlreg {
struct bazel_dep {
	std::string_view name;
	std::string_view version;
};

struct module_bazel {
	static auto parse( //
		std::string_view contents
	) -> std::optional<module_bazel>;

	std::string_view name;
	std::string_view version;
	int              compatibility_level;

	std::vector<bazel_dep> bazel_deps;
};
} // namespace bzlreg
