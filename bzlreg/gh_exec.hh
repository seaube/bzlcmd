#pragma once

#include <filesystem>

namespace bzlreg {

auto is_gh_available() -> bool;

auto gh_default_branch(std::string org, std::string repo)
	-> std::optional<std::string>;

auto gh_branch_commit_sha(std::string org, std::string repo, std::string branch)
	-> std::optional<std::string>;
} // namespace bzlreg
