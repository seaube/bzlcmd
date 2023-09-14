#include "bzlreg/decompress.hh"

#include <iostream>
#include "libdeflate.h"
#include "bzlreg/defer.hh"

using bzlreg::util::defer;

auto bzlreg::decompress_archive( //
	const std::vector<std::byte>& compressed_data
) -> std::vector<std::byte> {
	auto decomp = libdeflate_alloc_decompressor();
	auto _decomp_cleanup = defer([&] { libdeflate_free_decompressor(decomp); });

	auto decompressed_data = std::vector<std::byte>{};
	auto actual_decompressed_size = size_t{};

	auto guessed_decompressed_size = compressed_data.size() * 9;
	decompressed_data.resize(guessed_decompressed_size);

	auto decompress_result = libdeflate_gzip_decompress(
		decomp,
		compressed_data.data(),
		compressed_data.size(),
		decompressed_data.data(),
		decompressed_data.size(),
		&actual_decompressed_size
	);

	while(decompress_result == LIBDEFLATE_INSUFFICIENT_SPACE) {
		guessed_decompressed_size += compressed_data.size();
		decompressed_data.resize(guessed_decompressed_size);
		decompress_result = libdeflate_gzip_decompress(
			decomp,
			compressed_data.data(),
			compressed_data.size(),
			decompressed_data.data(),
			decompressed_data.size(),
			&actual_decompressed_size
		);
	}

	if(decompress_result != LIBDEFLATE_SUCCESS) {
		return {};
	}

	decompressed_data.resize(actual_decompressed_size);

	return decompressed_data;
}
