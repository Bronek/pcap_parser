#include "lib/error.hpp"
#include "lib/inputs.hpp"
#include "lib/stats.hpp"

#include <expected>
#include <filesystem>
#include <iostream>
#include <string>

using input_files = std::array<std::string, 2>;

// TODO: write a std::filesystem wrapper to make this testable.
auto find_inputs(std::string const &strpath) -> std::expected<input_files, error>
{
  namespace fs = std::filesystem;
  fs::path const path(strpath);
  if (!fs::exists(path)) {
    return error::make("path ", path.c_str(), " does not exist");
  }
  if (fs::status(path).type() != fs::file_type::directory) {
    return error::make("path ", path.c_str(), " is not a directory");
  }

  input_files result;
  int i = 0;
  for (auto const &direntry : fs::directory_iterator(path)) {
    if (i > 1) {
      return error::make("too many files in directory: ", path.c_str());
    }
    result[i++] = direntry.path().string();
  }
  if (i != 2) {
    return error::make("too few files in directory: ", path.c_str());
  }
  if (fs::status(result[0]).type() != fs::file_type::regular
      || fs::status(result[1]).type() != fs::file_type::regular) {
    return error::make("an input is not a regular file: ", result[0], ", ", result[1]);
  }

  return result;
}

// NOTE: Not used outside of main.cpp
struct exception final {
  int code;
};

[[noreturn]] void fail(int code, auto &&...args)
{
  ((std::cerr << args), ...);
  std::cerr << '\n';
  throw exception(code);
}

auto main(int argc, char const **argv) -> int
{
  try {
    if (argc != 2) {
      fail(13, "received ", argc - 1, " parameters but expected 1");
    }

    auto files = find_inputs(argv[1]);
    if (!files) {
      fail(14, files.error().what());
    }

    auto const sorted_files = sort(*files);
    if (!sorted_files) {
      fail(15, sorted_files.error());
    }

    auto pcap_inputs = PcapInputs::make(*sorted_files);
    if (!pcap_inputs) {
      fail(16, pcap_inputs.error());
    }

    auto const result = stats::make(*pcap_inputs);
    if (!result) {
      fail(17, result.error());
    }

    std::cout << *result << std::endl;
  } catch (exception e) {
    return e.code;
  } catch (std::exception const &e) {
    std::cerr << e.what() << '\n';
    return 2;
  }

  return 0;
}
