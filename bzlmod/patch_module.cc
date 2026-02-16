#include "bzlmod/patch_module.hh"

#include <filesystem>
#include <iostream>
#include <fstream>
#include <format>
#include <vector>
#include <string>
#include <optional>
#include <boost/process.hpp>
#include <boost/filesystem.hpp>
#include <nlohmann/json.hpp>
#include "bzlmod/get_registries.hh"
#include "bzlmod/find_workspace_dir.hh"
#include "bzlmod/download_module_metadata.hh"
#include "bzlreg/download.hh"
#include "bzlreg/decompress.hh"
#include "bzlreg/tar_view.hh"
#include "bzlreg/defer.hh"
#include "bzlreg/config_types.hh"
#include "bzlreg/unused.hh"

namespace bp = boost::process;
namespace fs = std::filesystem;
namespace bfs = boost::filesystem;
using json = nlohmann::json;
using namespace std::string_literals;

namespace {

auto get_bazel_info(const bfs::path& bazel_exe, std::string_view key) -> std::string {
	auto stdout_pipe = bp::ipstream{};
	auto proc = bp::child{
		bp::exe(bazel_exe),
		bp::args({"info"s, std::string{key}}),
		bp::std_out > stdout_pipe,
		bp::std_err > bp::null,
	};
	auto result = std::string{};
	if(!std::getline(stdout_pipe, result)) {
		result = "";
	}
	proc.wait();
	while(!result.empty() && (result.back() == '\r' || result.back() == '\n')) {
		result.pop_back();
	}
	return result;
}

auto get_module_version(const bfs::path& buildozer_exe, std::string_view module_name) -> std::string {
	auto stdout_pipe = bp::ipstream{};
	auto proc = bp::child{
		bp::exe(buildozer_exe),
		bp::args({
			"print version"s,
			std::format("//MODULE.bazel:{}", module_name)
		}),
		bp::std_out > stdout_pipe,
		bp::std_err > bp::null,
	};
	auto result = std::string{};
	if(!std::getline(stdout_pipe, result)) {
		result = "";
	}
	proc.wait();
	while(!result.empty() && (result.back() == '\r' || result.back() == '\n')) {
		result.pop_back();
	}
	if(result == "(missing)") {
		return "";
	}
	return result;
}

struct source_info {
	bzlreg::source_config config;
	std::string           registry_url;
};

auto download_source_info(
	const std::vector<std::string>& registries,
	std::string_view                module_name,
	std::string_view                version
) -> std::optional<source_info> {
	for(auto const& registry : registries) {
		auto url = std::format(
			"{}/modules/{}/{}/source.json",
			registry,
			module_name,
			version
		);
		auto data = bzlreg::download_file(url);
		if(data) {
			auto j = json::parse(std::string_view{
				reinterpret_cast<const char*>(data->data()),
				data->size()});
			return source_info{
				.config = j.get<bzlreg::source_config>(),
				.registry_url = registry,
			};
		}
	}
	return std::nullopt;
}

auto extract_archive_to(
	const std::vector<std::byte>& archive_data,
	const fs::path&               out_dir
) -> void {
	auto decompressed = bzlreg::decompress_archive(archive_data);
	auto tar = bzlreg::tar_view(decompressed);
	for(auto file : tar) {
		auto path = out_dir / file.name();
		if(file.name().ends_with('/')) {
			fs::create_directories(path);
			continue;
		}
		fs::create_directories(path.parent_path());
		auto out = std::ofstream{path, std::ios::binary};
		auto contents = file.contents();
		out.write(
			reinterpret_cast<const char*>(contents.data()),
			static_cast<std::streamsize>(contents.size())
		);
	}
}

} // namespace

auto bzlmod::patch_module(std::string_view module_name) -> int {
	auto buildozer = bp::search_path("buildozer");
	auto bazel = bp::search_path("bazel");
	auto git = bp::search_path("git");
	auto diff = bp::search_path("diff");
	auto patch = bp::search_path("patch");

	if(buildozer.empty() || bazel.empty()) {
		std::cerr << "[ERROR] `buildozer` and `bazel` are required to use `bzlmod patch`.\n";
		return 1;
	}

	auto workspace_dir = find_workspace_dir(fs::current_path());
	if(!workspace_dir) {
		std::cerr << "[ERROR] Could not find workspace directory.\n";
		return 1;
	}

	auto version = get_module_version(buildozer, module_name);
	if(version.empty()) {
		std::cerr << std::format(
			"[ERROR] Could not find version for module '{}' in MODULE.bazel\n",
			module_name
		);
		return 1;
	}

	auto registries = get_registries(*workspace_dir);
	if(!registries) {
		std::cerr << "[ERROR] Could not find registries.\n";
		return 1;
	}

	auto info = download_source_info(*registries, module_name, version);
	if(!info) {
		std::cerr << std::format(
			"[ERROR] Could not find source.json for {}@{}\n",
			module_name,
			version
		);
		return 1;
	}

	auto temp_dir = fs::temp_directory_path() / "bzlmod-patch" /
		std::format("{}-{}", module_name, version);
	auto ec = std::error_code{};
	fs::remove_all(temp_dir, ec);
	fs::create_directories(temp_dir);
	UNUSED(auto) =
		bzlreg::util::defer([&]() mutable { fs::remove_all(temp_dir, ec); });

	std::cout << "Downloading original source...\n";
	auto archive_data = bzlreg::download_file(info->config.url);
	if(!archive_data) {
		std::cerr << "[ERROR] Could not download archive.\n";
		return 1;
	}

	auto original_extract_dir = temp_dir / "original_raw";
	extract_archive_to(*archive_data, original_extract_dir);

	auto original_dir = original_extract_dir / info->config.strip_prefix;

	// Apply existing patches
	for(auto const& [patch_name, _] : info->config.patches) {
		auto patch_url = std::format(
			"{}/modules/{}/{}/patches/{}",
			info->registry_url,
			module_name,
			version,
			patch_name
		);
		auto patch_data = bzlreg::download_file(patch_url);
		if(!patch_data) {
			std::cerr << std::format("[ERROR] Could not download patch {}\n", patch_name);
			return 1;
		}
		auto patch_path = temp_dir / patch_name;
		{
			auto out = std::ofstream{patch_path, std::ios::binary};
			out.write(
				reinterpret_cast<const char*>(patch_data->data()),
				static_cast<std::streamsize>(patch_data->size())
			);
		}

		if(patch.empty()) {
			std::cerr << "[ERROR] `patch` command is required to apply existing patches.\n";
			return 1;
		}

		auto patch_proc = bp::child{
			bp::exe(patch),
			bp::start_dir(original_dir.generic_string()),
			bp::args({"-p1"s, "-i"s, patch_path.generic_string()}),
			bp::std_out > bp::null,
			bp::std_err > bp::null,
		};
		patch_proc.wait();
		if(patch_proc.exit_code() != 0) {
			std::cerr << std::format("[ERROR] Failed to apply patch {}\n", patch_name);
			return 1;
		}
	}

	// Apply overlays
	for(auto const& [file_name, _] : info->config.overlay) {
		auto file_url = std::format(
			"{}/modules/{}/{}/overlay/{}",
			info->registry_url,
			module_name,
			version,
			file_name
		);
		auto file_data = bzlreg::download_file(file_url);
		if(!file_data) {
			std::cerr << std::format(
				"[ERROR] Could not download overlay file {}\n",
				file_name
			);
			return 1;
		}
		auto file_path = original_dir / file_name;
		fs::create_directories(file_path.parent_path());
		auto out = std::ofstream{file_path, std::ios::binary};
		out.write(
			reinterpret_cast<const char*>(file_data->data()),
			static_cast<std::streamsize>(file_data->size())
		);
	}

	auto output_base = get_bazel_info(bazel, "output_base");
	auto external_dir = fs::path(output_base) / "external";
	auto edited_dir = external_dir / std::format("{}~{}", module_name, version);

	if(!fs::exists(edited_dir)) {
		// Try canonical name with '+' instead of '~' (Bazel 7+)
		edited_dir = external_dir / std::format("{}+{}", module_name, version);
	}

	if(!fs::exists(edited_dir)) {
		// Try just module_name+ (sometimes version is omitted in canonical name)
		edited_dir = external_dir / std::format("{}+", module_name);
	}

	if(!fs::exists(edited_dir)) {
		// Try just module_name~
		edited_dir = external_dir / std::format("{}~", module_name);
	}

	if(!fs::exists(edited_dir)) {
		// Try to find it by prefix as a fallback
		if(fs::exists(external_dir)) {
			for(auto const& entry : fs::directory_iterator{external_dir}) {
				auto name = entry.path().filename().string();
				if(entry.is_directory() &&
					 (name.starts_with(std::string(module_name) + "~") ||
					  name.starts_with(std::string(module_name) + "+"))) {
					edited_dir = entry.path();
					break;
				}
			}
		}
	}

	if(!fs::exists(edited_dir)) {
		std::cerr << std::format(
			"[ERROR] Could not find edited source in external directory: {}\n",
			edited_dir.generic_string()
		);
		return 1;
	}

	auto patches_dir = *workspace_dir / "patches";
	fs::create_directories(patches_dir);
	auto patch_name = std::format("{}+{}.patch", module_name, version);
	auto patch_file = patches_dir / patch_name;

	std::cout << "Generating patch...\n";

	// We use "a" and "b" as directory names for the patch to be applicable with -p1
	auto a_dir = temp_dir / "a";
	auto b_dir = temp_dir / "b";

	fs::rename(original_dir, a_dir);

	// On Windows, symlink might require admin or developer mode.
	// We'll try symlink first, then copy if it fails.
	auto symlink_ok = false;
	try {
		fs::create_directory_symlink(edited_dir, b_dir);
		symlink_ok = true;
	} catch(...) {
	}

	if(!symlink_ok) {
		fs::create_directories(b_dir);
		fs::copy(
			edited_dir,
			b_dir,
			fs::copy_options::recursive | fs::copy_options::overwrite_existing
		);
	}

	auto diff_proc = bp::child{};
	if(!git.empty()) {
		diff_proc = bp::child{
			bp::exe(git),
			bp::start_dir(temp_dir.generic_string()),
			bp::args({
				"diff"s,
				"--no-index"s,
				"a"s,
				"b"s,
			}),
			bp::std_out > patch_file.generic_string(),
		};
	} else if(!diff.empty()) {
		diff_proc = bp::child{
			bp::exe(diff),
			bp::start_dir(temp_dir.generic_string()),
			bp::args({
				"-ruN"s,
				"a"s,
				"b"s,
			}),
			bp::std_out > patch_file.generic_string(),
		};
	} else {
		std::cerr << "[ERROR] `git` or `diff` is required to generate patches.\n";
		return 1;
	}

	diff_proc.wait();
	// diff returns 1 if differences are found, which is what we expect.

	std::cout << std::format("Patch saved to patches/{}\n", patch_name);

	// Update MODULE.bazel
	std::cout << "Updating MODULE.bazel...\n";

	auto module_bazel_path = *workspace_dir / "MODULE.bazel";
	auto already_has_override = false;
	{
		auto in = std::ifstream{module_bazel_path, std::ios::binary};
		auto content = std::string{
			std::istreambuf_iterator<char>{in},
			std::istreambuf_iterator<char>{}
		};
		if(content.find(std::format("module_name = \"{}\"", module_name)) !=
			 std::string::npos &&
			 content.find("single_version_override") != std::string::npos) {
			already_has_override = true;
		}
	}

	if(already_has_override) {
		std::cout << std::format(
			"Warning: single_version_override for '{}' already exists in "
			"MODULE.bazel. Please ensure it uses the new patch manually.\n",
			module_name
		);
	} else {
		auto out = std::ofstream{module_bazel_path, std::ios::app};
		out << std::format(
			"\nsingle_version_override(\n"
			"    module_name = \"{}\",\n"
			"    version = \"{}\",\n"
			"    patches = [\"//:patches/{}\"],\n"
			"    patch_strip = 1,\n"
			")\n",
			module_name,
			version,
			patch_name
		);
	}

	return 0;
}
