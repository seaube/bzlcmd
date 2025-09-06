#include "bzlreg/tar_view.hh"

#include <string>
#include <format>
#include <string_view>
#include <iostream>
#include <cassert>
#include <charconv>

// https://en.wikipedia.org/wiki/Tar_(computing)
constexpr auto TAR_HEADER_SIZE = 512;
constexpr auto TAR_HEADER_FILE_NAME_MAX_LENGTH = 100;
constexpr auto TAR_HEADER_FILE_SIZE_OFFSET = 124;
constexpr auto TAR_HEADER_FILE_SIZE_LENGTH = 12;
constexpr auto TAR_HEADER_TYPE_FLAG_OFFSET = 156;
constexpr auto USTAR_HEADER_FILE_NAME_PREFIX_MAX_LENGTH = 155;

namespace {
enum class typeflag_enum : char {
	/**
	 * normal file can be NUL or 0
	 * use typeflag_is_normal_file() for checking
	 */
	normal_file_nul = '\0',

	/**
	 * normal file can be NUL or 0
	 * use typeflag_is_normal_file() for checking
	 */
	normal_file_zero = '0',

	hard_link = '1',
	symbolic_link = '2',
	character_special = '3',
	block_special = '4',
	directory = '5',
	fifo = '6',
	contiguous_file = '7',

	/**
	 * Global extended header with meta data (POSIX.1-2001)
	 */
	global_extended_header = 'g',

	/**
	 * Extended header with metadata for the next file in the archive
	 * (POSIX.1-2001)
	 */
	extended_header = 'x',
};

constexpr auto typeflag_is_normal_file(typeflag_enum v) -> bool {
	return v == typeflag_enum::normal_file_nul ||
		v == typeflag_enum::normal_file_zero;
}

constexpr auto get_typeflag(std::span<const std::byte> bytes) -> typeflag_enum {
	return static_cast<typeflag_enum>(bytes[TAR_HEADER_TYPE_FLAG_OFFSET]);
}

/**
 * Gets the 'file size' from the tar 512 byte header. This may be the actual
 * file size _or_ its the extended header size depending onn the typeflag.
 */
auto get_tar_header_file_size(std::span<const std::byte> bytes) -> std::size_t {
	auto size = std::size_t{0};
	auto [_, ec] = std::from_chars(
		reinterpret_cast<const char*>(bytes.data() + TAR_HEADER_FILE_SIZE_OFFSET),
		reinterpret_cast<const char*>(
			bytes.data() + TAR_HEADER_FILE_SIZE_OFFSET + TAR_HEADER_FILE_SIZE_LENGTH
		),
		size,
		8
	);

	if(ec != std::errc{}) {
		std::cerr << "ERROR: bad tar header file size: "
							<< std::make_error_code(ec).message() << std::endl;
		std::abort();
	}

	return size;
}

auto is_all0(std::span<const std::byte> bytes) -> std::size_t {
	for(const std::byte byte : bytes) {
		if(byte != std::byte{}) {
			return false;
		}
	}
	return true;
}
} // namespace

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

	while(get_typeflag(itr._data) == typeflag_enum::global_extended_header) {
		++itr;
	}

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
	auto header_size = operator*().header_byte_size();
	_data = _data.subspan(header_size + round_up_to_multiple(file_size, 512));
	if(is_all0(_data.subspan(0, TAR_HEADER_SIZE))) {
		_data = _data.subspan(TAR_HEADER_SIZE);

		if(is_all0(_data.subspan(0, TAR_HEADER_SIZE))) {
			_data = {};
		}
	}
	return *this;
}

auto bzlreg::tar_view::iterator::operator*() const -> tar_view_file {
	return tar_view_file{_data};
}

auto bzlreg::tar_view::file( //
	std::string_view find_filename
) -> tar_view_file {
	auto offset = size_t{0};
	auto consecutive_all0 = 0;

	while(offset < _tar_bytes.size()) {
		auto tar_header_bytes = _tar_bytes.subspan(offset, TAR_HEADER_SIZE);

		if(is_all0(tar_header_bytes)) {
			offset += TAR_HEADER_SIZE;
			consecutive_all0 += 1;

			if(consecutive_all0 >= 2) {
				// The end of an archive is marked by at least two consecutive
				// zero-filled records. SEE:
				// https://en.wikipedia.org/wiki/Tar_(computing)
				break;
			}
			continue;
		}

		consecutive_all0 = 0;

		auto file = tar_view_file{_tar_bytes.subspan(offset)};
		auto file_size = file.size();
		auto file_name = file.name();

		if(file_name == find_filename) {
			return file;
		}

		offset += file.header_byte_size() + round_up_to_multiple(file_size, 512);
	}

	return tar_view_file{};
}

bzlreg::tar_view_file::tar_view_file( //
	std::span<std::byte> data
) noexcept
	: _data(data) {
	if(_data.empty()) {
		return;
	}

	const auto typeflag = get_typeflag(_data);

	if(typeflag == typeflag_enum::extended_header) {
		auto extended_header = std::string_view{
			reinterpret_cast<const char*>(_data.data() + TAR_HEADER_SIZE),
			static_cast<std::size_t>(get_tar_header_file_size(_data)),
		};
		while(!extended_header.empty()) {
			auto length_sep_index = extended_header.find(' ');
			if(length_sep_index == std::string::npos) {
				std::cerr << "ERROR: bad extended header length separator" << std::endl;
				std::abort();
			}

			auto length = std::size_t{};
			auto [_, ec] = std::from_chars(
				extended_header.data(),
				extended_header.data() + length_sep_index,
				length
			);

			if(ec != std::errc{}) {
				std::cerr << "ERROR: bad extended header length: "
									<< std::make_error_code(ec).message() << std::endl;
				std::abort();
			}

			auto key_value_sep_index = extended_header.find('=', length_sep_index);
			if(key_value_sep_index == std::string::npos) {
				std::cerr << "ERROR: bad extended header key value pair separator"
									<< std::endl;
				std::abort();
			}

			auto key = extended_header.substr(
				length_sep_index,
				key_value_sep_index - length_sep_index
			);
			auto value = extended_header.substr(
				key_value_sep_index,
				length - (length_sep_index - key.size() - 2)
			);

			auto value_span = std::span{
				reinterpret_cast<const std::byte*>(value.data()),
				static_cast<std::size_t>(value.size()),
			};

			if(key == "path") {
				_extended_header_path = value_span;
			} else if(key == "linkpath") {
				_extended_header_linkpath = value_span;
			} else if(key == "size") {
				_extended_header_size = value_span;
			}
		}
	}
}

bzlreg::tar_view_file::tar_view_file() : _data{} {
}

bzlreg::tar_view_file::operator bool() const noexcept {
	return !_data.empty();
}

auto bzlreg::tar_view_file::header_byte_size() const -> size_t {
	assert(*this);

	const auto typeflag = get_typeflag(_data);

	if(typeflag == typeflag_enum::extended_header) {
		return TAR_HEADER_SIZE +
			round_up_to_multiple(get_tar_header_file_size(_data), 512);
	} else {
		return TAR_HEADER_SIZE;
	}
}

auto bzlreg::tar_view_file::name() const noexcept -> std::string {
	assert(*this);

	const auto typeflag = get_typeflag(_data);
	auto       name_str = std::string{};

	if(!_extended_header_path.empty()) {
		return std::string{
			reinterpret_cast<const char*>(_extended_header_path.data()),
			_extended_header_path.size(),
		};
	} else {
		auto name_cstr = reinterpret_cast<const char*>(_data.data());
		auto name_len = name_cstr[TAR_HEADER_FILE_NAME_MAX_LENGTH - 1] == '\0' //
			? std::strlen(name_cstr)
			: TAR_HEADER_FILE_NAME_MAX_LENGTH;
		name_str = std::string{name_cstr, name_len};
		auto maybe_ustar = name_cstr + 257;

		const auto is_ustar = strncmp(maybe_ustar, "ustar", 5) == 0 &&
			(maybe_ustar[5] == '\0' || maybe_ustar[5] == ' ');

		if(is_ustar) {
			auto name_prefix_cstr = name_cstr + 345;
			auto name_prefix_len = std::size_t{
				strnlen(name_prefix_cstr, USTAR_HEADER_FILE_NAME_PREFIX_MAX_LENGTH)
			};
			if(name_prefix_len > 0) {
				name_str = std::format(
					"{}/{}",
					std::string_view{name_prefix_cstr, name_prefix_len},
					name_str
				);
			} else {
			}
		}
	}

	return name_str;
}

auto bzlreg::tar_view_file::size() const noexcept -> size_t {
	assert(*this);

	const auto typeflag = get_typeflag(_data);

	if(typeflag == typeflag_enum::extended_header) {
		auto file_size = std::size_t{};
		auto [_, ec] = std::from_chars(
			reinterpret_cast<const char*>(_extended_header_size.data()),
			reinterpret_cast<const char*>(_extended_header_size.data()) +
				_extended_header_size.size(),
			file_size
		);
		if(ec != std::errc{}) {
			std::cerr << "ERROR: extended header file size: "
								<< std::make_error_code(ec).message() << std::endl;
			std::abort();
		}

		return file_size;
	} else {
		return get_tar_header_file_size(_data);
	}
}

auto bzlreg::tar_view_file::contents() const noexcept -> std::span<std::byte> {
	assert(*this);

	return std::span{
		_data.data() + header_byte_size(),
		size(),
	};
}

auto bzlreg::tar_view_file::string_view() const noexcept -> std::string_view {
	assert(*this);

	return std::string_view{
		reinterpret_cast<char*>(_data.data()) + header_byte_size(),
		size(),
	};
}
