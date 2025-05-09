#include "inputs.hpp"

#include <regex>

std::expected<pair<std::string>, error> sort(std::array<std::string, 2> const &files)
{
  pair<std::string> ret;

  if (files[0].empty()) {
    return error::make("first filename is empty");
  }
  if (files[1].empty()) {
    return error::make("second filename is empty");
  }
  if (files[0] == files[1]) {
    return error::make("both filenames are identical");
  }

  std::regex const regex(R"(^.*_([0-9]+)-[0-9]+.pcap$)");
  {
    std::smatch matches;
    std::regex_search(files[0], matches, regex);
    if (matches.size() == 2 && matches[1].str() == "14310") {
      ret.A = files[0];
    } else if (matches.size() == 2 && matches[1].str() == "15310") {
      ret.B = files[0];
    } else {
      return error::make("unexpected channel of first file: ", files[0]);
    }
  }

  {
    std::smatch matches;
    std::regex_search(files[1], matches, regex);
    std::string const expected = ret.A.empty() ? "14310" : "15310";
    if (matches.size() == 2 && matches[1].str() == expected) {
      (ret.A.empty() ? ret.A : ret.B) = files[1];
    } else {
      return error::make("unexpected channel of second file: ", files[1]);
    }
  }

  return ret;
}

