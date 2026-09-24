#include <filesystem>
#include <print>
#include "docoptexpr/docoptexpr.hh"
#include "bzlmod/init_module.hh"
#include "bzlmod/add_module.hh"
#include "bzlmod/update_module.hh"
#include "bzlmod/publish_module.hh"
#include "bzlmod/cc.hh"

namespace fs = std::filesystem;
using namespace docoptexpr::literals;

constexpr auto USAGE = R"(
Bzlmod - manage your bazel module with _ease_

Usage:
	bzlmod init [<module-dir>]
	bzlmod add <dep-name>
	bzlmod update
	bzlmod publish [--dry-run]
	bzlmod cc include_deps [<file>] [--fix]
	bzlmod cc list_headers [<label>] [--deps]
	bzlmod -h | --help

Options:
	--dry-run  Do everything except submit the pull request.
	--fix      Automatically add the missing dependencies.
	--deps     Include dependencies in the header listing.
	-h --help  Show this screen.
)"_docopt;

auto main(int argc, char* argv[]) -> int {
	auto bazel_working_dir = std::getenv("BUILD_WORKING_DIRECTORY");
	if(bazel_working_dir != nullptr) {
		fs::current_path(bazel_working_dir);
	}

	auto res = USAGE.parse(argc, argv);
	if(!res) {
		std::println(stderr, "Error matching arguments: {}", res.error());
		std::println(stderr, "{}", USAGE.usage());
		return 1;
	}

	auto args = res.value();
	auto exit_code = int{0};

	if(args.get<"--help">()) {
		std::println("{}", USAGE.help());
		return 0;
	}

	if(args.get<"init">()) {
		auto dir_sv = args.get<"<module-dir>">();
		auto module_dir = !dir_sv.empty() //
			? fs::path{dir_sv}
			: fs::current_path();

		exit_code = bzlmod::init_module(module_dir);
	} else if(args.get<"add">()) {
		auto dep_name = args.get<"<dep-name>">();
		exit_code = bzlmod::add_module(dep_name);
	} else if(args.get<"update">()) {
		exit_code = bzlmod::update_module();
	} else if(args.get<"publish">()) {
		auto dry_run = args.get<"--dry-run">();
		exit_code = bzlmod::publish_module(dry_run);
	} else if(args.get<"cc">() && args.get<"include_deps">()) {
		auto file = args.get<"<file>">();
		auto fix = args.get<"--fix">();
		exit_code = bzlmod::cc_include_deps(file, fix);
	} else if(args.get<"cc">() && args.get<"list_headers">()) {
		auto label = args.get<"<label>">();
		auto deps = args.get<"--deps">();
		exit_code = bzlmod::cc_list_headers(label, deps);
	}

	return exit_code;
}
