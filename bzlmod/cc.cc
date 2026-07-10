#include "bzlmod/cc.hh"

#include <iostream>
#include <fstream>
#include <regex>
#include <string>
#include <vector>
#include <set>
#include <optional>
#include <filesystem>
#include <print>
#include <sstream>

#define BOOST_PROCESS_VERSION 1
#include <boost/process/v1.hpp>
#include "nlohmann/json.hpp"
#include "bzlmod/find_workspace_dir.hh"

namespace bp = boost::process;
namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace std::string_literals;

static auto get_bazel_output_base() -> std::optional<fs::path> {
	auto bazel = bp::search_path("bazel");
	if(bazel.empty()) {
		return std::nullopt;
	}
	auto stdout_stream = bp::ipstream{};
	auto stderr_stream = bp::ipstream{};
	auto proc = bp::child{
		bp::exe(bazel.generic_string()),
		bp::args({"info"s, "output_base"s}),
		bp::std_out > stdout_stream,
		bp::std_err > stderr_stream,
		bp::std_in < bp::null,
	};
	auto output = std::string{};
	std::getline(stdout_stream, output);
	proc.wait();
	if (proc.exit_code() != 0 || output.empty()) {
		return std::nullopt;
	}
	while(!output.empty() && std::isspace(output.back())) {
		output.pop_back();
	}
	return fs::path{output};
}

static auto run_bazel_query(const std::string& query) -> std::optional<std::string> {
	auto bazel = bp::search_path("bazel");
	if(bazel.empty()) {
		return std::nullopt;
	}
	auto stdout_stream = bp::ipstream{};
	auto stderr_stream = bp::ipstream{};
	auto proc = bp::child{
		bp::exe(bazel.generic_string()),
		bp::args({"query"s, query}),
		bp::std_out > stdout_stream,
		bp::std_err > stderr_stream,
		bp::std_in < bp::null,
	};
	
	auto output = std::string{};
	auto line = std::string{};
	while(std::getline(stdout_stream, line)) {
		if(!line.empty()) {
			if(!output.empty()) output += "\n";
			output += line;
		}
	}
	proc.wait();
	if (proc.exit_code() != 0) {
		return std::nullopt;
	}
	return output;
}

static auto normalize_label(std::string_view label) -> std::string {
	std::string s{label};
	if (s.starts_with("//")) {
		return s;
	}
	if (s.starts_with("@")) {
		auto dbl_slash = s.find("//");
		if (dbl_slash == std::string::npos) {
			return s + "//:" + s.substr(1);
		}
		auto colon = s.find(':', dbl_slash);
		if (colon == std::string::npos) {
			auto pkg = s.substr(dbl_slash + 2);
			auto last_slash = pkg.find_last_of('/');
			auto target_name = (last_slash == std::string::npos) ? pkg : pkg.substr(last_slash + 1);
			return s + ":" + std::string{target_name};
		}
	}
	return s;
}

auto bzlmod::cc_include_deps(std::string_view file_path, bool fix) -> int {
	auto workspace_dir = find_workspace_dir(fs::current_path());
	if(!workspace_dir) {
		std::println(stderr, "[ERROR] Cannot find bazel workspace from {}.", fs::current_path().generic_string());
		return 1;
	}

	auto is_stdin = file_path.empty() || file_path == "-";
	if (fix && is_stdin) {
		std::println(stderr, "[ERROR] --fix only works if a file path is passed in, not stdin");
		return 1;
	}

	// 1. Read input
	std::vector<std::string> lines;
	if (is_stdin) {
		std::string line;
		while (std::getline(std::cin, line)) {
			lines.push_back(line);
		}
	} else {
		auto abs_file_path = fs::absolute(file_path);
		if (!fs::exists(abs_file_path)) {
			std::println(stderr, "[ERROR] File {} does not exist", file_path);
			return 1;
		}
		std::ifstream ifs{abs_file_path};
		std::string line;
		while (std::getline(ifs, line)) {
			lines.push_back(line);
		}
	}

	// 2. Parse includes
	std::regex include_re(R"(^\s*#\s*include\s+["<]([^">]+)[">])");
	std::set<std::string> include_paths;
	for (const auto& line : lines) {
		std::smatch match;
		if (std::regex_search(line, match, include_re)) {
			include_paths.insert(match[1].str());
		}
	}

	// 3. If fixing, find owner target and its current deps
	std::string owner_target;
	std::set<std::string> existing_deps;
	if (fix) {
		auto abs_file_path = fs::absolute(file_path);
		auto rel_file_path = fs::relative(abs_file_path, *workspace_dir);
		auto rel_file_path_str = rel_file_path.generic_string();

		auto owner_query_res = run_bazel_query(std::format("same_pkg_direct_rdeps({})", rel_file_path_str));
		if (owner_query_res && !owner_query_res->empty()) {
			std::stringstream ss(*owner_query_res);
			std::string first_line;
			if (std::getline(ss, first_line)) {
				while (!first_line.empty() && std::isspace(first_line.back())) first_line.pop_back();
				owner_target = first_line;
			}
		}

		if (owner_target.empty()) {
			std::println(stderr, "[ERROR] Could not find bazel target that owns {}", file_path);
			return 1;
		}

		auto deps_query_res = run_bazel_query(std::format("labels(deps, {})", owner_target));
		if (deps_query_res) {
			std::stringstream ss(*deps_query_res);
			std::string dep_line;
			while (std::getline(ss, dep_line)) {
				while (!dep_line.empty() && std::isspace(dep_line.back())) dep_line.pop_back();
				if (!dep_line.empty()) {
					existing_deps.insert(normalize_label(dep_line));
				}
			}
		}
	}

	// 4. Resolve each include path
	auto output_base = get_bazel_output_base();
	std::vector<fs::path> ext_dirs;
	if (output_base) {
		auto ext_path = *output_base / "external";
		if (fs::exists(ext_path)) {
			for (auto& entry : fs::directory_iterator(ext_path)) {
				if (entry.is_directory()) {
					ext_dirs.push_back(entry.path());
				}
			}
		}
	}

	std::vector<std::string> missing_deps_to_add;

	for (const auto& include_path_str : include_paths) {
		fs::path include_path{include_path_str};
		std::string found_target = "";

		// Check if it's local
		fs::path local_file_path;
		std::vector<fs::path> local_prefixes = {"", "src", "include"};
		for (const auto& prefix : local_prefixes) {
			fs::path path_to_check = prefix.empty() ? (*workspace_dir / include_path) : (*workspace_dir / prefix / include_path);
			if (fs::exists(path_to_check)) {
				local_file_path = path_to_check;
				break;
			}
		}

		if (!local_file_path.empty()) {
			auto rel_path = fs::relative(local_file_path, *workspace_dir);
			auto rel_path_str = rel_path.generic_string();
			auto file_label_res = run_bazel_query(rel_path_str);
			if (file_label_res && !file_label_res->empty()) {
				std::stringstream ss(*file_label_res);
				std::string file_label;
				std::getline(ss, file_label);
				while (!file_label.empty() && std::isspace(file_label.back())) file_label.pop_back();

				auto target_res = run_bazel_query(std::format("same_pkg_direct_rdeps({})", file_label));
				if (target_res && !target_res->empty()) {
					std::stringstream ss_target(*target_res);
					std::string target_label;
					std::getline(ss_target, target_label);
					while (!target_label.empty() && std::isspace(target_label.back())) target_label.pop_back();
					found_target = target_label;
				}
			}
		} else {
			// Search external
			for (const auto& dir : ext_dirs) {
				std::vector<fs::path> prefixes = {"", "include", "src"};
				fs::path found_file_path;
				fs::path matched_prefix;
				for (const auto& prefix : prefixes) {
					fs::path path_to_check = prefix.empty() ? (dir / include_path) : (dir / prefix / include_path);
					if (fs::exists(path_to_check)) {
						found_file_path = path_to_check;
						matched_prefix = prefix;
						break;
					}
				}

				if (!found_file_path.empty()) {
					fs::path ext_repo_dir = dir;
					fs::path file_rel_path = matched_prefix.empty() ? include_path : (matched_prefix / include_path);
					fs::path package_path = "";
					fs::path file_name = "";

					fs::path search_dir = file_rel_path.parent_path();
					while (true) {
						if (fs::exists(ext_repo_dir / search_dir / "BUILD.bazel") || fs::exists(ext_repo_dir / search_dir / "BUILD")) {
							package_path = search_dir;
							file_name = fs::relative(ext_repo_dir / file_rel_path, ext_repo_dir / package_path);
							break;
						}
						if (search_dir.empty() || search_dir == search_dir.root_path()) {
							file_name = file_rel_path;
							break;
						}
						search_dir = search_dir.parent_path();
					}

					std::string file_label;
					if (package_path.empty()) {
						file_label = std::format("@@{}//:{}", dir.filename().string(), file_name.generic_string());
					} else {
						file_label = std::format("@@{}//{}:{}", dir.filename().string(), package_path.generic_string(), file_name.generic_string());
					}

					auto target_res = run_bazel_query(std::format("same_pkg_direct_rdeps({})", file_label));
					if (target_res && !target_res->empty()) {
						std::stringstream ss_target(*target_res);
						std::string target_label;
						std::getline(ss_target, target_label);
						while (!target_label.empty() && std::isspace(target_label.back())) target_label.pop_back();
						found_target = target_label;
						break;
					}
				}
			}
		}

		std::string status = "not_found";
		if (!found_target.empty()) {
			auto norm_found = normalize_label(found_target);
			if (fix && existing_deps.contains(norm_found)) {
				status = "already_dep";
			} else {
				status = "found";
				if (fix) {
					missing_deps_to_add.push_back(found_target);
				}
			}
		}

		json j;
		j["include"] = include_path_str;
		if (found_target.empty()) {
			j["target"] = nullptr;
		} else {
			j["target"] = found_target;
		}
		j["status"] = status;

		std::println("{}", j.dump());
	}

	// 5. If fixing and we have missing deps, add them!
	if (fix && !missing_deps_to_add.empty()) {
		auto buildozer = bp::search_path("buildozer");
		if (buildozer.empty()) {
			std::println(stderr, "[ERROR] buildozer is required to add missing dependencies");
			return 1;
		}

		for (const auto& dep : missing_deps_to_add) {
			bp::child{
				bp::exe(buildozer.generic_string()),
				bp::args({
					std::format("add deps {}", dep),
					owner_target
				}),
				bp::std_out > bp::null,
				bp::std_err > bp::null,
			}.wait();
		}

		std::string pkg_name = "";
		if (owner_target.starts_with("//")) {
			auto colon = owner_target.find(':');
			if (colon != std::string::npos) {
				pkg_name = owner_target.substr(2, colon - 2);
			} else {
				pkg_name = owner_target.substr(2);
			}
		}

		auto build_file_dir = *workspace_dir / pkg_name;
		auto build_file_path = build_file_dir / "BUILD.bazel";
		if (!fs::exists(build_file_path)) {
			build_file_path = build_file_dir / "BUILD";
		}

		if (fs::exists(build_file_path)) {
			auto buildifier = bp::search_path("buildifier");
			if (!buildifier.empty()) {
				bp::child{
					bp::exe(buildifier.generic_string()),
					bp::args({build_file_path.generic_string()}),
					bp::std_out > bp::null,
					bp::std_err > bp::null,
				}.wait();
			}
		}
	}

	return 0;
}
