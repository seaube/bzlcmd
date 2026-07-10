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
	auto proc = bp::child{
		bp::exe(bazel.generic_string()),
		bp::args({"query"s, query}),
		bp::std_out > stdout_stream,
		bp::std_err > bp::null,
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

static auto run_bazel_query_keep_going(const std::string& query) -> std::optional<std::string> {
	auto bazel = bp::search_path("bazel");
	if(bazel.empty()) {
		return std::nullopt;
	}
	auto stdout_stream = bp::ipstream{};
	auto proc = bp::child{
		bp::exe(bazel.generic_string()),
		bp::args({"query"s, "--keep_going"s, "--output=build"s, query}),
		bp::std_out > stdout_stream,
		bp::std_err > bp::null,
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
	if (output.empty()) {
		return std::nullopt;
	}
	return output;
}

struct RuleLocation {
	std::string repo;
	std::string package;
};

static auto parse_location(const std::string& path_str, const fs::path& workspace_dir) -> RuleLocation {
	RuleLocation loc;
	fs::path p{path_str};
	auto generic_p = p.generic_string();

	auto ext_pos = generic_p.find("/external/");
	if (ext_pos != std::string::npos) {
		auto sub = generic_p.substr(ext_pos + 10);
		auto slash1 = sub.find('/');
		if (slash1 != std::string::npos) {
			loc.repo = sub.substr(0, slash1);
			auto sub2 = sub.substr(slash1 + 1);
			auto last_slash = sub2.find_last_of('/');
			if (last_slash != std::string::npos) {
				loc.package = sub2.substr(0, last_slash);
			} else {
				loc.package = "";
			}
		}
	} else {
		auto rel = fs::relative(p.parent_path(), workspace_dir);
		auto rel_str = rel.generic_string();
		if (rel_str == ".") {
			loc.package = "";
		} else {
			loc.package = rel_str;
		}
	}
	return loc;
}

static std::map<std::string, std::string> include_to_target_map;

static void parse_bazel_build_output(const std::string& output, const fs::path& workspace_dir) {
	std::stringstream ss(output);
	std::string line;
	std::string current_location_comment = "";
	bool in_rule = false;
	std::string rule_type = "";
	std::string rule_name = "";
	std::vector<std::string> rule_hdrs;
	std::string strip_include_prefix = "";
	std::string include_prefix = "";
	std::vector<std::string> rule_includes;

	auto parse_string_list = [](const std::string& val) -> std::vector<std::string> {
		std::vector<std::string> res;
		std::regex label_re("\"([^\"]+)\"");
		auto words_begin = std::sregex_iterator(val.begin(), val.end(), label_re);
		auto words_end = std::sregex_iterator();
		for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
			res.push_back((*i)[1].str());
		}
		return res;
	};

	auto parse_string_value = [](const std::string& val) -> std::string {
		std::regex str_re("\"([^\"]*)\"");
		std::smatch m;
		if (std::regex_search(val, m, str_re)) {
			return m[1].str();
		}
		return "";
	};

	auto process_current_rule = [&]() {
		if (rule_name.empty()) return;
		RuleLocation loc = parse_location(current_location_comment, workspace_dir);
		
		std::string rule_label;
		if (loc.repo.empty()) {
			rule_label = std::format("//{}:{}", loc.package, rule_name);
		} else {
			std::string apparent_repo = loc.repo;
			auto plus = apparent_repo.find('+');
			if (plus != std::string::npos) apparent_repo = apparent_repo.substr(0, plus);
			auto tilde = apparent_repo.find('~');
			if (tilde != std::string::npos) apparent_repo = apparent_repo.substr(0, tilde);
			rule_label = std::format("@{}//{}:{}", apparent_repo, loc.package, rule_name);
		}

		for (const auto& hdr_label : rule_hdrs) {
			std::string hdr_path = "";
			auto dbl_slash = hdr_label.find("//");
			if (dbl_slash != std::string::npos) {
				auto pkg_part = hdr_label.substr(dbl_slash + 2);
				auto colon = pkg_part.find(':');
				if (colon != std::string::npos) {
					auto pkg = pkg_part.substr(0, colon);
					auto file = pkg_part.substr(colon + 1);
					if (pkg.empty()) {
						hdr_path = file;
					} else {
						hdr_path = pkg + "/" + file;
					}
				}
			}

			if (hdr_path.empty()) continue;

			std::string inc_path = hdr_path;

			if (!strip_include_prefix.empty()) {
				if (strip_include_prefix.starts_with("/")) {
					auto prefix = strip_include_prefix.substr(1) + "/";
					if (inc_path.starts_with(prefix)) {
						inc_path = inc_path.substr(prefix.length());
					}
				} else {
					auto prefix = loc.package.empty() ? (strip_include_prefix + "/") : (loc.package + "/" + strip_include_prefix + "/");
					if (inc_path.starts_with(prefix)) {
						inc_path = inc_path.substr(prefix.length());
					}
				}
			}
			
			for (const auto& inc : rule_includes) {
				auto prefix = loc.package.empty() ? (inc + "/") : (loc.package + "/" + inc + "/");
				if (inc_path.starts_with(prefix)) {
					inc_path = inc_path.substr(prefix.length());
					break;
				}
			}

			if (!include_prefix.empty()) {
				inc_path = include_prefix + "/" + inc_path;
			}

			include_to_target_map[inc_path] = rule_label;
		}
	};

	while (std::getline(ss, line)) {
		while (!line.empty() && std::isspace(line.front())) line.erase(line.begin());
		while (!line.empty() && std::isspace(line.back())) line.pop_back();

		if (line.starts_with("# ")) {
			current_location_comment = line.substr(2);
			continue;
		}

		if (line.ends_with("(")) {
			process_current_rule();

			rule_type = line.substr(0, line.length() - 1);
			rule_name = "";
			rule_hdrs.clear();
			strip_include_prefix = "";
			include_prefix = "";
			rule_includes.clear();
			in_rule = true;
			continue;
		}

		if (line == ")") {
			process_current_rule();
			in_rule = false;
			rule_name = "";
			continue;
		}

		if (in_rule) {
			auto eq = line.find('=');
			if (eq != std::string::npos) {
				auto key = line.substr(0, eq);
				while (!key.empty() && std::isspace(key.back())) key.pop_back();
				auto val = line.substr(eq + 1);

				if (key == "name") {
					rule_name = parse_string_value(val);
				} else if (key == "hdrs" || key == "srcs") {
					if (val.find(']') == std::string::npos) {
						std::string accumulated = val;
						std::string sub_line;
						while (std::getline(ss, sub_line)) {
							accumulated += "\n" + sub_line;
							if (sub_line.find(']') != std::string::npos) {
								break;
							}
						}
						auto list = parse_string_list(accumulated);
						rule_hdrs.insert(rule_hdrs.end(), list.begin(), list.end());
					} else {
						auto list = parse_string_list(val);
						rule_hdrs.insert(rule_hdrs.end(), list.begin(), list.end());
					}
				} else if (key == "strip_include_prefix") {
					strip_include_prefix = parse_string_value(val);
				} else if (key == "include_prefix") {
					include_prefix = parse_string_value(val);
				} else if (key == "includes") {
					rule_includes = parse_string_list(val);
				}
			}
		}
	}
	process_current_rule(); // Catch last rule
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

	// 4. Build exact include mapping from Bazel query output
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

	auto query_res = run_bazel_query_keep_going("kind('cc_.*', deps(//...))");
	if (query_res) {
		parse_bazel_build_output(*query_res, *workspace_dir);
	}

	std::vector<std::string> missing_deps_to_add;

	for (const auto& include_path_str : include_paths) {
		fs::path include_path{include_path_str};
		std::string found_target = "";

		// Look in our exact include map first
		auto it = include_to_target_map.find(include_path_str);
		if (it != include_to_target_map.end()) {
			found_target = it->second;
		}

		// Fallback to heuristic filesystem search if not found in map
		if (found_target.empty()) {
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

auto bzlmod::cc_list_headers() -> int {
	auto workspace_dir = find_workspace_dir(fs::current_path());
	if(!workspace_dir) {
		std::println(stderr, "[ERROR] Cannot find bazel workspace from {}.", fs::current_path().generic_string());
		return 1;
	}

	include_to_target_map.clear();

	auto query_res = run_bazel_query_keep_going("kind('cc_.*', deps(//...))");
	if (query_res) {
		parse_bazel_build_output(*query_res, *workspace_dir);
	}

	for (const auto& [header, target] : include_to_target_map) {
		std::println(R"({{"header":"{}","target":"{}"}})", header, target);
	}

	return 0;
}
