#include "bzlmod/publish_module.hh"

#include <filesystem>
#include <print>
#include <fstream>
#include <chrono>
#include <vector>
#include <optional>
#include <string>
#include <string_view>
#define BOOST_PROCESS_VERSION 1
#include <boost/process/v1.hpp>
#include "absl/strings/str_split.h"
#include "absl/strings/ascii.h"
#include "nlohmann/json.hpp"
#include "bzlmod/find_workspace_dir.hh"
#include "bzlreg/module_bazel.hh"
#include "bzlreg/gh_exec.hh"
#include "bzlreg/add_module.hh"
#include "bzlreg/download.hh"
#include "bzlreg/decompress.hh"
#include "bzlreg/tar_view.hh"
#include "bzlreg/defer.hh"

namespace fs = std::filesystem;
namespace bp = boost::process;
using json = nlohmann::json;
using bzlreg::util::defer;

struct presubmit_config {
	std::vector<std::string> build_targets;
	std::vector<std::string> test_targets;
};

static auto parse_presubmit_yaml(const fs::path& path) -> presubmit_config {
	auto config = presubmit_config{};
	auto file = std::ifstream{path};
	if(!file.is_open()) {
		return config;
	}
	auto line = std::string{};
	bool in_build_targets = false;
	bool in_test_targets = false;
	while(std::getline(file, line)) {
		auto trimmed = absl::StripAsciiWhitespace(line);
		if(trimmed.empty()) {
			continue;
		}
		if(trimmed.starts_with("#")) {
			continue;
		}
		if(trimmed.starts_with("build_targets:")) {
			in_build_targets = true;
			in_test_targets = false;
			continue;
		} else if(trimmed.starts_with("test_targets:")) {
			in_build_targets = false;
			in_test_targets = true;
			continue;
		} else if(
			trimmed.find(':') != std::string::npos && !trimmed.starts_with("-")
		) {
			in_build_targets = false;
			in_test_targets = false;
		}

		if(trimmed.starts_with("-")) {
			auto target = trimmed.substr(1);
			target = absl::StripAsciiWhitespace(target);
			if(
				(target.starts_with("\"") && target.ends_with("\"")) ||
				(target.starts_with("'") && target.ends_with("'"))
			) {
				target = target.substr(1, target.size() - 2);
			}
			if(in_build_targets) {
				config.build_targets.push_back(std::string{target});
			} else if(in_test_targets) {
				config.test_targets.push_back(std::string{target});
			}
		}
	}
	return config;
}

struct github_repo_info {
	std::string org;
	std::string repo;
};

static auto get_git_remote_url(const fs::path& repo_dir)
	-> std::optional<std::string> {
	auto git_exe = bp::search_path("git");
	if(git_exe.empty()) {
		return std::nullopt;
	}
	auto git_out = bp::ipstream{};
	auto git_proc = bp::child{
		bp::exe(git_exe),
		bp::start_dir(repo_dir.generic_string()),
		bp::args({"remote", "get-url", "origin"}),
		bp::std_out > git_out,
		bp::std_in < bp::null,
	};
	std::string url;
	std::getline(git_out, url);
	git_proc.wait();
	if(git_proc.exit_code() != 0 || url.empty()) {
		return std::nullopt;
	}
	return std::string{absl::StripAsciiWhitespace(url)};
}

static auto parse_github_remote(std::string_view url)
	-> std::optional<github_repo_info> {
	if(url.find("github.com") == std::string::npos) {
		return std::nullopt;
	}

	std::string org, repo;
	if(url.starts_with("git@github.com:")) {
		auto                     path = url.substr(15);
		std::vector<std::string> parts = absl::StrSplit(path, '/');
		if(parts.size() >= 2) {
			org = parts[0];
			repo = parts[1];
		}
	} else if(url.starts_with("https://github.com/")) {
		auto                     path = url.substr(19);
		std::vector<std::string> parts = absl::StrSplit(path, '/');
		if(parts.size() >= 2) {
			org = parts[0];
			repo = parts[1];
		}
	} else {
		auto gh_pos = url.find("github.com");
		auto rest = url.substr(gh_pos + 10);
		if(!rest.empty() && (rest[0] == '/' || rest[0] == ':')) {
			rest = rest.substr(1);
		}
		std::vector<std::string> parts = absl::StrSplit(rest, '/');
		if(parts.size() >= 2) {
			org = parts[0];
			repo = parts[1];
		}
	}

	if(repo.ends_with(".git")) {
		repo = repo.substr(0, repo.size() - 4);
	}

	if(org.empty() || repo.empty()) {
		return std::nullopt;
	}

	return github_repo_info{org, repo};
}

static auto get_matching_tag(const fs::path& repo_dir, std::string_view version)
	-> std::string {
	auto git_exe = bp::search_path("git");
	if(git_exe.empty()) {
		return std::string{version};
	}
	auto check_tag = [&](const std::string& tag) -> bool {
		auto git_proc = bp::child{
			bp::exe(git_exe),
			bp::start_dir(repo_dir.generic_string()),
			bp::args(
				{"rev-parse", "-q", "--verify", std::format("refs/tags/{}", tag)}
			),
			bp::std_out > bp::null,
			bp::std_err > bp::null
		};
		git_proc.wait();
		return git_proc.exit_code() == 0;
	};

	auto v_tag = std::format("v{}", version);
	if(check_tag(v_tag)) {
		return v_tag;
	}
	auto normal_tag = std::string{version};
	if(check_tag(normal_tag)) {
		return normal_tag;
	}
	return normal_tag;
}

static auto extract_tar(bzlreg::tar_view tar_view, const fs::path& dest_dir)
	-> bool {
	auto ec = std::error_code{};
	for(auto f : tar_view) {
		auto name = f.name();
		if(name.empty()) {
			continue;
		}
		auto out_path = dest_dir / name;
		if(name.ends_with('/')) {
			fs::create_directories(out_path, ec);
			continue;
		}
		fs::create_directories(out_path.parent_path(), ec);
		auto out_file = std::ofstream{out_path, std::ios::binary};
		if(!out_file) {
			std::println(
				stderr,
				"ERROR: failed to open file for writing: {}",
				out_path.generic_string()
			);
			return false;
		}
		auto contents = f.contents();
		out_file.write(
			reinterpret_cast<const char*>(contents.data()),
			contents.size()
		);
	}
	return true;
}

auto bzlmod::publish_module(bool dry_run) -> int {
	if(!bzlreg::is_gh_available()) {
		std::println(
			stderr,
			"ERROR: 'gh' CLI tool is required but not found in PATH."
		);
		return 1;
	}

	auto workspace_dir = find_workspace_dir(fs::current_path());
	if(!workspace_dir) {
		std::println(
			stderr,
			"ERROR: Cannot find bazel workspace from {}.",
			fs::current_path().generic_string()
		);
		return 1;
	}
	auto module_bazel_path = *workspace_dir / "MODULE.bazel";
	if(!fs::exists(module_bazel_path)) {
		std::println(
			stderr,
			"ERROR: MODULE.bazel not found in workspace: {}",
			workspace_dir->generic_string()
		);
		return 1;
	}

	auto module_bzl_stream = std::ifstream{module_bazel_path};
	auto module_bzl_content = std::string{
		std::istreambuf_iterator<char>{module_bzl_stream},
		std::istreambuf_iterator<char>{}
	};

	auto module_info = bzlreg::module_bazel::parse(module_bzl_content);
	if(!module_info || module_info->name.empty() || module_info->version.empty()) {
		std::println(
			stderr,
			"ERROR: failed to parse module name or version from MODULE.bazel"
		);
		return 1;
	}

	auto remote_url = get_git_remote_url(*workspace_dir);
	if(!remote_url) {
		std::println(stderr, "ERROR: failed to find git remote URL for origin");
		return 1;
	}

	auto gh_info = parse_github_remote(*remote_url);
	if(!gh_info) {
		std::println(
			stderr,
			"ERROR: only GitHub repositories are supported for publishing. Remote "
			"URL: {}",
			*remote_url
		);
		return 1;
	}

	auto tag = get_matching_tag(*workspace_dir, module_info->version);
	auto archive_url = std::format(
		"https://github.com/{}/{}/archive/refs/tags/{}.tar.gz",
		gh_info->org,
		gh_info->repo,
		tag
	);

	std::println(
		"Publishing module {}@{}",
		module_info->name,
		module_info->version
	);
	std::println("Inferred archive URL: {}", archive_url);

	auto ec = std::error_code{};
	auto now_ms = std::chrono::steady_clock::now().time_since_epoch().count();
	auto temp_bcr_dir =
		fs::temp_directory_path() / std::format("bzlmod-bcr-{}", now_ms);
	auto temp_src_dir =
		fs::temp_directory_path() / std::format("bzlmod-src-{}", now_ms);

	auto cleanup_bcr = defer([&]() {
		if(!dry_run) {
			fs::remove_all(temp_bcr_dir, ec);
		}
	});
	auto cleanup_src = defer([&]() { fs::remove_all(temp_src_dir, ec); });

	// Clone BCR
	std::println("Cloning bazel-central-registry...");
	auto git_exe = bp::search_path("git");
	if(git_exe.empty()) {
		std::println(stderr, "ERROR: git is required but not found in PATH");
		return 1;
	}

	auto clone_proc = bp::child{
		bp::exe(git_exe),
		bp::args(
			{"clone",
			 "--depth",
			 "1",
			 "https://github.com/bazelbuild/bazel-central-registry.git",
			 temp_bcr_dir.generic_string()}
		)
	};
	clone_proc.wait();
	if(clone_proc.exit_code() != 0) {
		std::println(stderr, "ERROR: failed to clone bazel-central-registry");
		return 1;
	}

	// Add module to registry using bzlreg
	std::println("Adding module entry using bzlreg...");
	auto add_exit_code = bzlreg::add_module({
		.registry_dir = temp_bcr_dir,
		.archive_url = archive_url,
		.strip_prefix = "",
	});

	if(add_exit_code != 0) {
		std::println(stderr, "ERROR: failed to add module entry using bzlreg");
		return 1;
	}

	auto module_name = std::string{module_info->name};
	auto module_version = std::string{module_info->version};

	auto source_json_path =
		temp_bcr_dir / "modules" / module_name / module_version / "source.json";
	if(!fs::exists(source_json_path)) {
		std::println(
			stderr,
			"ERROR: source.json was not created at expected path: {}",
			source_json_path.generic_string()
		);
		return 1;
	}

	auto source_json = json::parse(std::ifstream{source_json_path});
	auto strip_prefix = source_json.at("strip_prefix").get<std::string>();

	// Handle presubmit.yml
	auto presubmit_dest_path =
		temp_bcr_dir / "modules" / module_name / module_version / "presubmit.yml";
	auto local_presubmit_path = *workspace_dir / ".bazelci/presubmit.yml";
	if(!fs::exists(local_presubmit_path)) {
		local_presubmit_path = *workspace_dir / "presubmit.yml";
	}

	if(fs::exists(local_presubmit_path)) {
		std::println(
			"Copying local presubmit.yml from {}...",
			local_presubmit_path.generic_string()
		);
		fs::copy_file(
			local_presubmit_path,
			presubmit_dest_path,
			fs::copy_options::overwrite_existing,
			ec
		);
	} else {
		std::println("No local presubmit.yml found. Generating a default one...");
		auto default_presubmit =
			std::ofstream{presubmit_dest_path, std::ios::binary};
		default_presubmit << R"(matrix:
  platform:
    - ubuntu2004
tasks:
  ubuntu2004:
    platform: ubuntu2004
    build_targets:
      - "//..."
    test_targets:
      - "//..."
)";
	}

	// Parse targets from presubmit.yml
	auto presubmit = parse_presubmit_yaml(presubmit_dest_path);
	if(presubmit.build_targets.empty() && presubmit.test_targets.empty()) {
		std::println(
			"No targets found in presubmit.yml. Falling back to build/test //..."
		);
		presubmit.build_targets.push_back("//...");
		presubmit.test_targets.push_back("//...");
	}

	// Download & extract archive to temp_src_dir for simulation
	std::println(
		"Downloading and extracting archive for local presubmit simulation..."
	);
	auto compressed_data = bzlreg::download_file(archive_url);
	if(!compressed_data) {
		std::println(stderr, "ERROR: failed to download archive for verification");
		return 1;
	}
	auto decompressed_data = bzlreg::decompress_archive(*compressed_data);
	if(decompressed_data.empty()) {
		std::println(stderr, "ERROR: failed to decompress archive data");
		return 1;
	}

	auto tar_view = bzlreg::tar_view{decompressed_data};
	fs::create_directories(temp_src_dir, ec);
	if(!extract_tar(tar_view, temp_src_dir)) {
		std::println(stderr, "ERROR: failed to extract archive files");
		return 1;
	}

	auto test_workspace_root = temp_src_dir / strip_prefix;
	if(
		!fs::exists(test_workspace_root / "WORKSPACE") &&
		!fs::exists(test_workspace_root / "WORKSPACE.bazel")
	) {
		std::ofstream{test_workspace_root / "WORKSPACE"} << "\n";
	}

	// Set up local registry override in .bazelrc or command line
	std::println("Simulating presubmit...");
	auto bazel_exe = bp::search_path("bazel");
	if(bazel_exe.empty()) {
		std::println(stderr, "ERROR: bazel is required but not found in PATH");
		return 1;
	}

	auto run_bazel_cmd = [&](
												 std::string_view                subcommand,
												 const std::vector<std::string>& targets
											 ) -> int {
		auto args = std::vector<std::string>{
			std::format("--output_base={}", (temp_src_dir / "ob").generic_string()),
			std::string{subcommand}
		};

		auto local_registry_path = fs::absolute(temp_bcr_dir).generic_string();
		if(local_registry_path.starts_with("/")) {
			args.push_back(std::format("--registry=file://{}", local_registry_path));
		} else {
			args.push_back(std::format("--registry=file:///{}", local_registry_path));
		}
		args.push_back("--registry=https://bcr.bazel.build");

		for(const auto& t : targets) {
			args.push_back(t);
		}

		auto bazel_proc = bp::child{
			bp::exe(bazel_exe),
			bp::start_dir(test_workspace_root.generic_string()),
			bp::args(args)
		};
		bazel_proc.wait();
		return bazel_proc.exit_code();
	};

	if(!presubmit.build_targets.empty()) {
		std::print("Simulating build targets:");
		for(const auto& t : presubmit.build_targets) {
			std::print(" {}", t);
		}
		std::println();

		auto exit_code = run_bazel_cmd("build", presubmit.build_targets);
		if(exit_code != 0) {
			std::println(stderr, "ERROR: simulated bazel build failed");
			return 1;
		}
	}

	if(!presubmit.test_targets.empty()) {
		std::print("Simulating test targets:");
		for(const auto& t : presubmit.test_targets) {
			std::print(" {}", t);
		}
		std::println();

		auto exit_code = run_bazel_cmd("test", presubmit.test_targets);
		if(exit_code != 0) {
			std::println(stderr, "ERROR: simulated bazel test failed");
			return 1;
		}
	}

	// Shutdown simulated bazel server to release resources
	bp::child{
		bp::exe(bazel_exe),
		bp::start_dir(test_workspace_root.generic_string()),
		bp::args(
			{std::format("--output_base={}", (temp_src_dir / "ob").generic_string()),
			 "shutdown"}
		)
	}
		.wait();

	if(dry_run) {
		std::println("Dry run enabled. Skipping commit and PR creation.");
		std::println("Verification was successful!");
		std::println("Registry directory: {}", temp_bcr_dir.generic_string());
		std::println(
			"To submit the PR to the BCR, run the following commands in the registry "
			"directory:"
		);
		std::println(
			"  git checkout -b publish-{}-{}",
			module_name,
			module_version
		);
		std::println("  git add .");
		std::println(
			"  git commit -m \"Publish {}@{}\"",
			module_name,
			module_version
		);
		std::println(
			"  gh pr create --title \"Publish {}@{}\" --body \"Publish {}@{} via "
			"bzlmod publish\"",
			module_name,
			module_version,
			module_name,
			module_version
		);
		return 0;
	}

	// Create git branch, commit, and submit PR
	std::println(
		"Verification successful. Committing changes and creating pull request..."
	);

	auto branch_name = std::format("publish-{}-{}", module_name, module_version);

	auto git_run = [&](const std::vector<std::string>& args) -> int {
		auto proc = bp::child{
			bp::exe(git_exe),
			bp::start_dir(temp_bcr_dir.generic_string()),
			bp::args(args)
		};
		proc.wait();
		return proc.exit_code();
	};

	if(git_run({"checkout", "-b", branch_name}) != 0) {
		std::println(
			stderr,
			"ERROR: failed to git checkout branch {}",
			branch_name
		);
		return 1;
	}
	if(git_run({"add", "."}) != 0) {
		std::println(stderr, "ERROR: failed to git add registry changes");
		return 1;
	}
	if(
		git_run(
			{"commit",
			 "-m",
			 std::format("Publish {}@{}", module_name, module_version)}
		) != 0
	) {
		std::println(stderr, "ERROR: failed to git commit registry changes");
		return 1;
	}

	// Run gh pr create interactively
	std::println("Opening PR using 'gh'...");
	auto gh_exe = bp::search_path("gh");
	auto pr_proc = bp::child{
		bp::exe(gh_exe),
		bp::start_dir(temp_bcr_dir.generic_string()),
		bp::args({
			"pr",
			"create",
			"--title",
			std::format("Publish {}@{}", module_name, module_version),
			"--body",
			std::format(
				"Publish {}@{} via bzlmod publish",
				module_name,
				module_version
			),
		}),
	};
	pr_proc.wait();
	if(pr_proc.exit_code() != 0) {
		std::println(stderr, "ERROR: failed to create pull request");
		return 1;
	}

	std::println("Successfully published {}@{}!", module_name, module_version);
	return 0;
}
