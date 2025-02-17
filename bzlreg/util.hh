#pragma once

#include <filesystem>
#include <string>
#include <system_error>
#include <optional>
#include <span>

namespace bzlreg {
auto calc_integrity( //
	std::span<const std::byte> data
) -> std::optional<std::string>;

auto calc_source_integrity(std::filesystem::path source_json) -> void;

template<typename CharContainer>
auto read_file_contents(
	std::filesystem::path path,
	CharContainer&        out_container,
	std::error_code&      ec
) -> void {
	using size_type = typename CharContainer::size_type;

	ec = {};

	::FILE* fp = ::fopen(path.string().c_str(), "rb");
	if(!fp) {
		ec = make_error_code(std::errc::no_such_file_or_directory);
		return;
	}
	::fseek(fp, 0, SEEK_END);
	auto sz = ::ftell(fp);
	if(sz == 0) {
		ec = make_error_code(std::errc::io_error);
		return;
	}
	out_container.resize(static_cast<size_type>(sz));
	if(sz) {
		::rewind(fp);
		sz = ::fread(std::data(out_container), 1, std::size(out_container), fp);
		if(sz == 0) {
			ec = make_error_code(std::errc::io_error);
			return;
		}
	}
	::fclose(fp);
}
} // namespace bzlreg
