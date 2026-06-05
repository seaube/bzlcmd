#include <filesystem>
#include <print>
#include "docoptexpr/docoptexpr.hh"
#include "bzlreg/init_registry.hh"
#include "bzlreg/bazel_exec.hh"
#include "bzlreg/add_module.hh"
#include "bzlreg/calc_integrity.hh"

namespace fs = std::filesystem;
using namespace docoptexpr::literals;

constexpr auto USAGE = R"(
Bazel registry CLI utility

Usage:
	bzlreg init [<registry-dir>]
	bzlreg build <label> [--registry=<path>]
	bzlreg test <label> [--registry=<path>]
	bzlreg run <label> [--registry=<path>]
	bzlreg add-module <archive-url> [--strip-prefix=<str>] [--registry=<path>]
	bzlreg calc-integrity <module> [--strip-prefix=<str>] [--registry=<path>]
	bzlreg -h | --help

Options:
	--registry=<path>     Registry directory. Defaults to current working directory.
	--strip-prefix=<str>  Prefix stripped from archive and set in source.json.
	-h --help             Show this screen.
)"_docopt;

using ArgsType = decltype(USAGE)::result_type;

static auto forward_bazel_subcommand(
	const ArgsType&  options,
	std::string_view subcommand
) -> int {
	auto registry_sv = options.get<"--registry">();
	auto registry_dir = !registry_sv.empty() //
		? std::optional<fs::path>{registry_sv}
		: std::nullopt;
	auto label = options.get<"<label>">();
	return bzlreg::bazel_exec({
		.registry_dir = registry_dir,
		.label = label,
		.subcommand = subcommand,
	});
}

static auto calc_integrity_command(const ArgsType& options) -> int {
	auto registry_sv = options.get<"--registry">();
	auto registry_dir = !registry_sv.empty() //
		? fs::path{registry_sv}
		: fs::current_path();
	auto module = options.get<"<module>">();

	return bzlreg::calc_integrity({
		.registry_dir = registry_dir,
		.module_name = std::string{module},
	});
}

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
		auto registry_sv = args.get<"<registry-dir>">();
		auto registry_dir = !registry_sv.empty() //
			? fs::path{registry_sv}
			: fs::current_path();

		exit_code = bzlreg::init_registry(registry_dir);
	} else if(args.get<"build">()) {
		exit_code = forward_bazel_subcommand(args, "build");
	} else if(args.get<"test">()) {
		exit_code = forward_bazel_subcommand(args, "test");
	} else if(args.get<"run">()) {
		exit_code = forward_bazel_subcommand(args, "run");
	} else if(args.get<"calc-integrity">()) {
		exit_code = calc_integrity_command(args);
	} else if(args.get<"add-module">()) {
		auto strip_prefix = args.get<"--strip-prefix">();
		auto registry_sv = args.get<"--registry">();
		auto registry_dir = !registry_sv.empty() //
			? fs::path{registry_sv}
			: fs::current_path();

		if(registry_dir.empty()) {
			std::println(stderr, "[ERROR] --registry must not be empty");
			std::println(stderr, "{}", USAGE.help());
			return 1;
		}

		auto archive_url = args.get<"<archive-url>">();
		exit_code = bzlreg::add_module({
			.registry_dir = registry_dir,
			.archive_url = archive_url,
			.strip_prefix = strip_prefix,
		});
	}

	return exit_code;
}
