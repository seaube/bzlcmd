#pragma once

#include <concepts>

namespace bzlreg::util {
auto defer(std::invocable auto&& fn) {
	struct defer_result_t {
		decltype(fn) _cleanup_fn;

		~defer_result_t() {
			_cleanup_fn();
		}
	};

	return defer_result_t{std::move(fn)};
}
} // namespace bzlreg::util
