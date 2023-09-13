#include "bzlreg/download.hh"

#include <boost/process.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <string>

namespace bp = boost::process;
using namespace std::string_literals;

auto bzlreg::download_archive( //
	std::string_view url
) -> std::vector<std::byte> {
	// TODO(zaucy): replace with libcurl or libcpr
	auto curl = bp::search_path("curl");
	if(curl.empty()) {
		return {};
	}

	auto ioc = boost::asio::io_context{};
	auto curl_stdout = std::future<std::vector<char>>();
	auto curl_proc = bp::child{
		ioc,
		bp::exe(curl),
		bp::args({"-sL"s, std::string{url}}),
		bp::std_out > curl_stdout,
		bp::std_in.close(),
	};

	ioc.run();
	curl_proc.wait();

	if(auto exit_code = curl_proc.exit_code(); exit_code != 0) {
		return {};
	}

	auto data = curl_stdout.get();
	static_assert(sizeof(decltype(data)::value_type) == sizeof(std::byte));
	return reinterpret_cast<std::vector<std::byte>&>(data); // don't @ me
}
