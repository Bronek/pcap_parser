#include <catch2/catch_all.hpp>

#include "lib/dummy.hpp"

TEST_CASE("Dummy")
{ //
  CHECK(dummy::foo == 12);
}
