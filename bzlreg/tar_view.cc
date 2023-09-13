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

auto bzlreg::tar_view::file( //
	std::string_view find_filename
) -> tar_view_file {
	auto offset = size_t{0};

	while(offset < _tar_bytes.size()) {
		auto file = tar_view_file(_tar_bytes.data() + offset);
		auto file_size = file.size();
		auto file_name = file.name();

		if(file_name == find_filename) {
			return file;
		}

		offset += TAR_HEADER_SIZE + round_up_to_multiple(file_size, 512);
	}

	return tar_view_file{nullptr};
}

bzlreg::tar_view_file::tar_view_file( //
	std::byte* data
) noexcept
	: _data(data) {
}

bzlreg::tar_view_file::operator bool() const noexcept {
	return _data != nullptr;
}

auto bzlreg::tar_view_file::name() const noexcept -> std::string_view {
	assert(*this);

	auto name_cstr = reinterpret_cast<char*>(_data);
	auto name_len = name_cstr[TAR_HEADER_FILE_NAME_MAX_LENGTH - 1] == '\0' //
		? std::strlen(name_cstr)
		: TAR_HEADER_FILE_SIZE_LENGTH;
	return std::string_view{name_cstr, name_len};
}

auto bzlreg::tar_view_file::size() const noexcept -> size_t {
	assert(*this);

	auto size = std::size_t{0};
	auto result = std::from_chars(
		reinterpret_cast<const char*>(_data + TAR_HEADER_FILE_SIZE_OFFSET),
		reinterpret_cast<const char*>(
			_data + TAR_HEADER_FILE_SIZE_OFFSET + TAR_HEADER_FILE_SIZE_LENGTH
		),
		size,
		8
	);
	return size;
}

auto bzlreg::tar_view_file::contents() const noexcept -> std::span<std::byte> {
	assert(*this);

	return std::span{_data + TAR_HEADER_SIZE, size()};
}

auto bzlreg::tar_view_file::string_view() const noexcept -> std::string_view {
	assert(*this);

	return std::string_view{
		reinterpret_cast<char*>(_data) + TAR_HEADER_SIZE,
		size(),
	};
}
