#pragma once

#include <optional>
#include <vector>
#include <string>

namespace bzlreg {
struct bazel_dep {
	std::string name;
	std::string version;
};

struct module_bazel {
	static auto parse( //
		std::string_view contents
	) -> std::optional<module_bazel>;

	std::string name;
	std::string version;
	int         compatibility_level;

	std::vector<bazel_dep> bazel_deps;
};
} // namespace bzlreg
