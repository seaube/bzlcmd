#include "bzlreg/gh_exec.hh"

#include <iostream>
#include <filesystem>
#include <fstream>
#include <boost/process.hpp>

namespace bp = boost::process;
namespace fs = std::filesystem;
using namespace std::string_literals;

auto bzlreg::is_gh_available() -> bool {
	return !bp::search_path("gh").empty();
}

auto bzlreg::gh_default_branch(std::string org, std::string repo)
	-> std::optional<std::string> {
	auto gh = bp::search_path("gh");
	if(gh.empty()) {
		return {};
	}

	auto gh_proc_stdout = bp::ipstream{};
	auto gh_proc_stderr = bp::ipstream{};
	auto gh_proc = bp::child{
		bp::exe(gh.string()),
		bp::args({
			"api"s,
			std::format("repos/{}/{}", org, repo),
			"--jq"s,
			".default_branch"s,
		}),
		bp::std_out > gh_proc_stdout,
		bp::std_err > gh_proc_stderr,
		bp::std_in < bp::null,
	};

	auto line = std::string{};
	while(std::getline(gh_proc_stderr, line)) {
		if(line.empty()) {
			std::cerr << std::format("ERROR: {}\n", line);
		}
	}

	std::getline(gh_proc_stdout, line);

	gh_proc.wait();

	auto proc_exit_code = gh_proc.exit_code();
	if(proc_exit_code != 0) {
		return {};
	}

	if(line.empty()) {
		return {};
	}

	return line;
}

auto bzlreg::gh_branch_commit_sha(
	std::string org,
	std::string repo,
	std::string branch
) -> std::optional<std::string> {
	auto gh = bp::search_path("gh");
	if(gh.empty()) {
		return {};
	}

	auto gh_proc_stdout = bp::ipstream{};
	auto gh_proc_stderr = bp::ipstream{};
	auto gh_proc = bp::child{
		bp::exe(gh.generic_string()),
		bp::args({
			"api"s,
			std::format("repos/{}/{}/git/ref/heads/{}", org, repo, branch),
			"--jq"s,
			".object.sha"s,
		}),
		bp::std_out > gh_proc_stdout,
		bp::std_err > gh_proc_stderr,
		bp::std_in < bp::null,
	};

	auto line = std::string{};
	while(std::getline(gh_proc_stderr, line)) {
		if(line.empty()) {
			std::cerr << std::format("ERROR: {}\n", line);
		}
	}

	std::getline(gh_proc_stdout, line);

	gh_proc.wait();

	auto proc_exit_code = gh_proc.exit_code();
	if(proc_exit_code != 0) {
		return {};
	}

	if(line.empty()) {
		return {};
	}

	return line;
}

auto bzlreg::gh_commit_date(
	std::string org,
	std::string repo,
	std::string commit
) -> std::optional<std::string> {
	auto gh = bp::search_path("gh");
	if(gh.empty()) {
		return {};
	}

	auto gh_proc_stdout = bp::ipstream{};
	auto gh_proc_stderr = bp::ipstream{};
	auto gh_proc = bp::child{
		bp::exe(gh.generic_string()),
		bp::args({
			"api"s,
			std::format("repos/{}/{}/git/commits/{}", org, repo, commit),
			"--jq"s,
			".author.date"s,
		}),
		bp::std_out > gh_proc_stdout,
		bp::std_err > gh_proc_stderr,
		bp::std_in < bp::null,
	};

	auto line = std::string{};
	while(std::getline(gh_proc_stderr, line)) {
		if(line.empty()) {
			std::cerr << std::format("ERROR: {}\n", line);
		}
	}

	std::getline(gh_proc_stdout, line);

	gh_proc.wait();

	auto proc_exit_code = gh_proc.exit_code();
	if(proc_exit_code != 0) {
		return {};
	}

	if(line.empty()) {
		return {};
	}

	return line;
}
