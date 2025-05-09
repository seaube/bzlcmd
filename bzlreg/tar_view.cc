#include "bzlreg/tar_view.hh"

#include <string>
#include <string_view>
#include <iostream>
#include <cassert>
#include <charconv>

// https://en.wikipedia.org/wiki/Tar_(computing)
constexpr auto TAR_HEADER_SIZE = 512;
constexpr auto TAR_HEADER_FILE_NAME_MAX_LENGTH = 100;
constexpr auto TAR_HEADER_FILE_SIZE_OFFSET = 124;
constexpr auto TAR_HEADER_FILE_SIZE_LENGTH = 12;
constexpr auto USTAR_HEADER_FILE_NAME_PREFIX_MAX_LENGTH = 155;

bzlreg::tar_view::tar_view(tar_view&&) = default;
bzlreg::tar_view::tar_view(const tar_view&) = default;
bzlreg::tar_view_file::tar_view_file(tar_view_file&&) noexcept = default;
bzlreg::tar_view_file::tar_view_file(const tar_view_file&) noexcept = default;

bzlreg::tar_view::tar_view(std::span<std::byte> tar_bytes)
	: _tar_bytes(tar_bytes) {
}

bzlreg::tar_view::~tar_view() {
}

static size_t round_up_to_multiple( //
	size_t num,
	size_t multiple
) {
	assert(multiple && ((multiple & (multiple - 1)) == 0));
	return (num + multiple - 1) & -multiple;
}

auto bzlreg::tar_view::begin() -> iterator {
	auto itr = iterator{};
	itr._data = _tar_bytes;
	return itr;
}

auto bzlreg::tar_view::end() const noexcept -> sentinel {
	return {};
}

auto bzlreg::tar_view::iterator::operator!=(sentinel) const -> bool {
	if(_data.empty()) {
		return false;
	}

	auto name = operator*().name();
	return !name.empty();
}

auto bzlreg::tar_view::iterator::operator++() -> iterator& {
	assert(!_data.empty());
	auto file_size = operator*().size();
	_data = _data.subspan(TAR_HEADER_SIZE + round_up_to_multiple(file_size, 512));
	return *this;
}

auto bzlreg::tar_view::iterator::operator*() const -> tar_view_file {
	return tar_view_file{_data};
}

auto bzlreg::tar_view::file( //
	std::string_view find_filename
) -> tar_view_file {
	auto offset = size_t{0};

	while(offset < _tar_bytes.size()) {
		auto file = tar_view_file{_tar_bytes.subspan(offset)};
		auto file_size = file.size();
		auto file_name = file.name();

		if(file_name == find_filename) {
			return file;
		}

		offset += TAR_HEADER_SIZE + round_up_to_multiple(file_size, 512);
	}

	return tar_view_file{};
}

bzlreg::tar_view_file::tar_view_file( //
	std::span<std::byte> data
) noexcept
	: _data(data) {
}

bzlreg::tar_view_file::tar_view_file() : _data{} {
}

bzlreg::tar_view_file::operator bool() const noexcept {
	return !_data.empty();
}

auto bzlreg::tar_view_file::name() const noexcept -> std::string {
	assert(*this);

	auto name_cstr = reinterpret_cast<char*>(_data.data());
	auto name_len = name_cstr[TAR_HEADER_FILE_NAME_MAX_LENGTH - 1] == '\0' //
		? std::strlen(name_cstr)
		: TAR_HEADER_FILE_SIZE_LENGTH;
	auto name_str = std::string{name_cstr, name_len};
	auto maybe_ustar = name_cstr + 257;

	if(maybe_ustar[5] == '\0' && strcmp(maybe_ustar, "ustar") == 0) {
		auto name_prefix_cstr = name_cstr + 345;
		auto name_prefix_len = strlen(name_prefix_cstr);
		if(name_prefix_len > 0) {
			auto name_prefix_str = std::string{name_prefix_cstr, name_prefix_len};
			return name_prefix_str + "/" + name_str;
		}
	}

	return name_str;
}

auto bzlreg::tar_view_file::size() const noexcept -> size_t {
	assert(*this);

	auto size = std::size_t{0};
	auto result = std::from_chars(
		reinterpret_cast<const char*>(_data.data() + TAR_HEADER_FILE_SIZE_OFFSET),
		reinterpret_cast<const char*>(
			_data.data() + TAR_HEADER_FILE_SIZE_OFFSET + TAR_HEADER_FILE_SIZE_LENGTH
		),
		size,
		8
	);
	return size;
}

auto bzlreg::tar_view_file::contents() const noexcept -> std::span<std::byte> {
	assert(*this);

	return std::span{_data.data() + TAR_HEADER_SIZE, size()};
}

auto bzlreg::tar_view_file::string_view() const noexcept -> std::string_view {
	assert(*this);

	return std::string_view{
		reinterpret_cast<char*>(_data.data()) + TAR_HEADER_SIZE,
		size(),
	};
}
