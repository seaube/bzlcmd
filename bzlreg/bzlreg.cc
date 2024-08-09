#include <filesystem>
#include <iostream>
#include "docopt.h"
#include "bzlreg/init_registry.hh"
#include "bzlreg/add_module.hh"
#include "ftxui/component/captured_mouse.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/loop.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

namespace fs = std::filesystem;

constexpr auto USAGE = R"docopt(
Bazel registry CLI utility

Usage:
	bzlreg init [<registry-dir>]
	bzlreg add-module [<archive-url>...] [--registry=<path>]

Options:
	--registry=<path>         Registry directory. Defaults to current working directory.
	-s, --strip-prefix=<str>  Prefix stripped from archive and set in source.json.
)docopt";

auto main(int argc, char* argv[]) -> int {
	auto bazel_working_dir = std::getenv("BUILD_WORKING_DIRECTORY");
	if(bazel_working_dir != nullptr) {
		fs::current_path(bazel_working_dir);
	}

	auto args = docopt::docopt(USAGE, {argv + 1, argv + argc});
	auto exit_code = int{0};

	if(args["init"].asBool()) {
		auto registry_dir = args["<registry-dir>"] //
			? fs::path{args.at("<registry-dir>").asString()}
			: fs::current_path();

		exit_code = bzlreg::init_registry(registry_dir);
	} else if(args["add-module"].asBool()) {
		auto cancelled = false;
		auto archive_urls = args["<archive-url>"].asStringList();
		auto main_archive_url = std::string{};
		auto main_archive_url_index = 0;

		if(archive_urls.size() > 1) {
			auto screen = ftxui::ScreenInteractive::TerminalOutput();
			auto radiobox = ftxui::Radiobox(ftxui::RadioboxOption{
				.entries = &archive_urls,
				.selected = &main_archive_url_index,
			});
			auto confirm_btn = ftxui::Button(
				"Confirm",
				[&] { screen.Exit(); },
				ftxui::ButtonOption::Ascii()
			);
			auto cancel_btn = ftxui::Button(
				"Cancel",
				[&] {
					screen.Exit();
					cancelled = true;
				},
				ftxui::ButtonOption::Ascii()
			);
			auto container = ftxui::Container::Vertical({
				radiobox,
				ftxui::Container::Horizontal({confirm_btn, cancel_btn}),
			});

			auto renderer = ftxui::Renderer(container, [&]() -> ftxui::Element {
				return ftxui::vbox({
								 ftxui::hbox(ftxui::text("Pick Main Archive")),
								 ftxui::separator(),
								 container->Render(),
							 }) |
					ftxui::border;
			});

			screen.Loop(renderer);
		}

		if(cancelled) {
			return 1;
		}

		main_archive_url = archive_urls[main_archive_url_index];
		archive_urls.erase(archive_urls.begin() + main_archive_url_index);

		auto registry_dir = args["--registry"] //
			? fs::path{args.at("--registry").asString()}
			: fs::current_path();

		if(registry_dir.empty()) {
			std::cerr << "[ERROR] --registry must not be empty\n";
			std::cerr << USAGE;
			return 1;
		}

		auto archive_url = args.at("<archive-url>").asString();
		exit_code = bzlreg::add_module({
			.registry_dir = registry_dir,
			.main_archive_url = archive_urls[main_archive_url_index],
			.supplement_archive_urls = archive_urls,
		});
	}

	return exit_code;
}
