#include "sort_channels.hpp"

#include <regex>

std::string const channel_A = "14310";
std::string const channel_B = "15310";

auto sort_channels_t::operator()(std::array<std::string, 2> const &files) const
    -> std::expected<pair<std::string>, error>
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

  // NOTE: Assumption that channel is encoded as a penultimate group of numbers, '_' on one side and '-' on the other
  std::regex const regex(R"(^.*_([0-9]+)-[0-9]+.pcap$)");
  {
    std::smatch matches;
    std::regex_search(files[0], matches, regex);
    if (matches.size() == 2 && matches[1].str() == channel_A) {
      ret.A = files[0];
    } else if (matches.size() == 2 && matches[1].str() == channel_B) {
      ret.B = files[0];
    } else {
      return error::make("unexpected channel of first file: ", files[0]);
    }
  }

  {
    std::smatch matches;
    std::regex_search(files[1], matches, regex);
    std::string const expected = ret.A.empty() ? channel_A : channel_B;
    if (matches.size() == 2 && matches[1].str() == expected) {
      (ret.A.empty() ? ret.A : ret.B) = files[1];
    } else {
      return error::make("unexpected channel of second file: ", files[1]);
    }
  }

  return ret;
}

