#include "bzlreg/module_bazel.hh"

#include <unordered_map>
#include <string>
#include <string_view>
#include <iostream>
#include <charconv>
#include "absl/strings/str_split.h"
#include "absl/strings/ascii.h"

struct parse_call_result {
	std::string_view                                       name;
	std::unordered_map<std::string_view, std::string_view> attrs;
	std::string_view                                       contents_after;
};

static auto parse_call( //
	std::string_view contents
) -> parse_call_result {
	auto paren_open = contents.find('(');
	if(paren_open == std::string::npos) {
		return {};
	}
	auto paren_close = contents.find(')', paren_open);
	if(paren_close == std::string::npos) {
		return {};
	}

	auto result = parse_call_result{};
	result.name = absl::StripAsciiWhitespace(contents.substr(0, paren_open));

	std::vector<std::string_view> attr_lines = absl::StrSplit(
		contents.substr(paren_open + 1, paren_close - paren_open - 1),
		',',
		absl::SkipWhitespace()
	);

	for(auto attr_line : attr_lines) {
		std::vector<std::string_view> attr_line_split = absl::StrSplit(
			attr_line,
			absl::MaxSplits('=', 1),
			absl::SkipWhitespace()
		);

		if(attr_line_split.size() > 1) {
			auto attr_name = absl::StripAsciiWhitespace(attr_line_split[0]);
			auto attr_value = absl::StripAsciiWhitespace(attr_line_split[1]);

			result.attrs[attr_name] = attr_value;
		}
	}

	result.contents_after = contents.substr(paren_close + 1);

	return result;
}

static auto attr_as_string(std::string_view attr_value) -> std::string_view {
	return attr_value.substr(1, attr_value.size() - 2);
}

static auto attr_as_int(std::string_view attr_value) -> int {
	int attr_num = 0;
	std::from_chars(
		attr_value.data(),
		attr_value.data() + attr_value.size(),
		attr_num
	);

	return attr_num;
}

auto bzlreg::module_bazel::parse( //
	std::string_view contents
) -> std::optional<module_bazel> {
	auto result = parse_call(contents);

	if(result.name != "module") {
		// TODO(zaucy): report this error
		return std::nullopt;
	}

	auto mod = module_bazel{};
	mod.name = attr_as_string(result.attrs.at("name"));
	if(result.attrs.contains("version")) {
		mod.version = attr_as_string(result.attrs.at("version"));
	}
	if(result.attrs.contains("compatibility_level")) {
		mod.compatibility_level =
			attr_as_int(result.attrs.at("compatibility_level"));
	} else {
		mod.compatibility_level = 1;
	}

	while(!absl::StripAsciiWhitespace(result.contents_after).empty()) {
		result = parse_call(result.contents_after);

		if(result.name == "bazel_dep") {
			if(!result.attrs.contains("name")) {
				continue;
			}
			if(!result.attrs.contains("version")) {
				continue;
			}

			mod.bazel_deps.emplace_back(
				std::string{attr_as_string(result.attrs.at("name"))},
				std::string{attr_as_string(result.attrs.at("version"))}
			);
		}
	}

	return mod;
}
