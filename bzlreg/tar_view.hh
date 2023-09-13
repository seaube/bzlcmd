#pragma once

#include <span>
#include <cstddef>
#include <string_view>

namespace bzlreg {
class tar_view;

class tar_view_file {
	friend tar_view;
	std::byte* _data;

	tar_view_file(std::byte* data) noexcept;

public:
	tar_view_file(tar_view_file&&) noexcept;
	tar_view_file(const tar_view_file&) noexcept;

	operator bool() const noexcept;

	auto name() const noexcept -> std::string_view;
	auto size() const noexcept -> size_t;
	auto contents() const noexcept -> std::span<std::byte>;
	auto string_view() const noexcept -> std::string_view;
};

class tar_view {
	std::span<std::byte> _tar_bytes;

public:
	tar_view(std::span<std::byte> tar_bytes);
	tar_view(const tar_view& other);
	tar_view(tar_view&& other);
	~tar_view();

	auto file(std::string_view filename) -> tar_view_file;
};

} // namespace bzlreg
