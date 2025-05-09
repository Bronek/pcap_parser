#include <catch2/catch_all.hpp>

#include <string>

#include "lib/inputs.hpp"

TEST_CASE("sorting of inputs")
{
  SECTION("invalid inputs")
  {
    CHECK(sort({"", ""}).error() == "first filename is empty");
    CHECK(sort({"a", ""}).error() == "second filename is empty");

    CHECK(sort({"a", "a"}).error() == "both filenames are identical");
    CHECK(sort({"_14310-0.pcap", "_14310-0.pcap"}).error() == "both filenames are identical");

    CHECK(sort({"a", "b"}).error() == "unexpected channel of first file: a");

    CHECK(sort({"_14310-0.pcap", "b"}).error() == "unexpected channel of second file: b");
    CHECK(sort({"_15310-0.pcap", "b"}).error() == "unexpected channel of second file: b");
    CHECK(sort({"_14310-0.pcap", "R_14310-0.pcap"}).error().what()
          == "unexpected channel of second file: R_14310-0.pcap");
    CHECK(sort({"_15310-0.pcap", "R_15310-0.pcap"}).error().what()
          == "unexpected channel of second file: R_15310-0.pcap");
  }

  SECTION("valid inputs")
  {
    using T = pair<std::string>;
    CHECK(sort({"_14310-0.pcap", "_15310-0.pcap"}).value() == T{.A = "_14310-0.pcap", .B = "_15310-0.pcap"});
    CHECK(sort({"_15310-0.pcap", "_14310-0.pcap"}).value() == T{.A = "_14310-0.pcap", .B = "_15310-0.pcap"});
  }
}
