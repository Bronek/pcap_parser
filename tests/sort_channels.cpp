#include <catch2/catch_all.hpp>

#include <string>

#include "lib/sort_channels.hpp"

TEST_CASE("sorting of channels")
{
  SECTION("invalid inputs")
  {
    CHECK(sort_channels({"", ""}).error() == "first filename is empty");
    CHECK(sort_channels({"a", ""}).error() == "second filename is empty");

    CHECK(sort_channels({"a", "a"}).error() == "both filenames are identical");
    CHECK(sort_channels({"_14310-0.pcap", "_14310-0.pcap"}).error() == "both filenames are identical");

    CHECK(sort_channels({"a", "b"}).error() == "unexpected channel of first file: a");

    CHECK(sort_channels({"_14310-0.pcap", "b"}).error() == "unexpected channel of second file: b");
    CHECK(sort_channels({"_15310-0.pcap", "b"}).error() == "unexpected channel of second file: b");
    CHECK(sort_channels({"_14310-0.pcap", "R_14310-0.pcap"}).error().what()
          == "unexpected channel of second file: R_14310-0.pcap");
    CHECK(sort_channels({"_15310-0.pcap", "R_15310-0.pcap"}).error().what()
          == "unexpected channel of second file: R_15310-0.pcap");
  }

  SECTION("valid inputs")
  {
    using T = pair<std::string>;
    CHECK(sort_channels({"_14310-0.pcap", "_15310-0.pcap"}).value() == T{.A = "_14310-0.pcap", .B = "_15310-0.pcap"});
    CHECK(sort_channels({"_15310-0.pcap", "_14310-0.pcap"}).value() == T{.A = "_14310-0.pcap", .B = "_15310-0.pcap"});
  }
}
