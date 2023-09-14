#include "bzlmod/get_registries.hh"

#include <string>
#include <fstream>
#include "absl/strings/str_split.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_replace.h"

namespace fs = std::filesystem;
using namespace std::string_literals;
using namespace std::string_view_literals;

constexpr auto BAZEL_CENTRAL_REGISTRY = "https://bcr.bazel.build";

auto is_bazelrc_comment(std::string_view line) -> bool {
	return line.starts_with("#");
}

auto get_bazelrc_registry_flag_value( //
	std::string_view line
) -> std::optional<std::string_view> {
	std::vector<std::string_view> line_split = absl::StrSplit( //
		line,
		"--registry=",
		absl::SkipWhitespace()
	);

	if(line_split.size() > 1) {
		return line_split[1];
	}
	return std::nullopt;
}

auto get_bazelrc_import( //
	std::string_view line
) -> std::optional<std::string_view> {
	constexpr auto import_prefixes = std::array{
		"try-import"sv,
		"import"sv,
	};

	for(auto import_prefix : import_prefixes) {
		if(line.starts_with(import_prefix)) {
			return absl::StripAsciiWhitespace(line.substr(import_prefix.size()));
		}
	}

	return std::nullopt;
}

struct parse_bazelrc_result {
	std::vector<std::string> registries;
	std::vector<std::string> imports;
};

static auto parse_bazelrc( //
	fs::path bazelrc_path
) -> parse_bazelrc_result {
	auto result = parse_bazelrc_result{};
	auto bazelrc_file = std::ifstream{bazelrc_path};

	auto line = std::string{};
	while(bazelrc_file && std::getline(bazelrc_file, line)) {
		auto stripped_line = absl::StripAsciiWhitespace(line);
		if(stripped_line.empty()) {
			continue;
		}
		if(is_bazelrc_comment(stripped_line)) {
			continue;
		}

		if(auto imp = get_bazelrc_import(stripped_line); imp) {
			result.imports.emplace_back(*imp);
		} else if(auto reg = get_bazelrc_registry_flag_value(stripped_line); reg) {
			result.registries.emplace_back(*reg);
		}
	}

	return result;
}

static auto parse_bazelrc_recursive(
	fs::path bazelrc_path,
	fs::path workspace_dir
) -> parse_bazelrc_result {
	auto result = parse_bazelrc(bazelrc_path);

	auto already_has_import = [&](auto imp) -> bool {
		for(auto existing_imp : result.imports) {
			if(imp == existing_imp) {
				return true;
			}
		}

		return false;
	};

	for(auto imp : result.imports) {
		if(already_has_import(imp)) {
			continue;
		}

		auto imp_path = std::string{imp};
		absl::StrReplaceAll(
			{
				{"%workspace%", workspace_dir.generic_string()},
			},
			&imp_path
		);
		auto imp_result = parse_bazelrc_recursive(imp_path, workspace_dir);
		result.registries.insert(
			result.registries.end(),
			imp_result.registries.begin(),
			imp_result.registries.end()
		);
		result.imports.insert(
			result.imports.end(),
			imp_result.imports.begin(),
			imp_result.imports.end()
		);
	}

	return result;
}

static auto get_default_bazelrc_paths( //
	fs::path workspace_dir
) -> std::vector<fs::path> {
	auto default_bazelrc_paths = std::vector{
		workspace_dir / ".bazelrc",
	};

#ifdef _WIN32
	auto home_dir = std::getenv("USERPROFILE");
	auto system_rc_dir = std::getenv("ProgramData");
#else
	auto home_dir = std::getenv("HOME");
	auto system_rc_dir = "/etc";
#endif

	if(home_dir != nullptr) {
		default_bazelrc_paths.push_back(fs::path{home_dir} / ".bazelrc");
	}

#ifdef BAZEL_SYSTEM_BAZELRC_PATH
	default_bazelrc_paths.push_back(fs::path{BAZEL_SYSTEM_BAZELRC_PATH});
#else
	if(system_rc_dir != nullptr) {
		default_bazelrc_paths.push_back(fs::path{system_rc_dir} / "bazel.bazelrc");
	}
#endif

	return default_bazelrc_paths;
}

auto bzlmod::get_registries( //
	fs::path workspace_dir
) -> std::optional<std::vector<std::string>> {
	auto registries = std::vector<std::string>{};
	auto default_bazelrc_paths = get_default_bazelrc_paths(workspace_dir);

	for(auto bazelrc_path : default_bazelrc_paths) {
		auto result = parse_bazelrc_recursive(bazelrc_path, workspace_dir);
		registries.insert(
			registries.end(),
			result.registries.begin(),
			result.registries.end()
		);
	}

	if(registries.empty()) {
		registries.push_back(BAZEL_CENTRAL_REGISTRY);
	}

	return registries;
}
