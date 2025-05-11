#include <catch2/catch_all.hpp>

#include <net/ethernet.h>
#include <netinet/in.h>

#include "mock_inputs.hpp"
#include "packet_tools.hpp"

#include "lib/stats.hpp"

TEST_CASE("average calc")
{
  stats const zero{.packet_count = {}, .dropped_count = {}, .faster_count = {}, .advantage_total_ns = {}};
  using T = pair<double>;
  SECTION("zeros")
  { //
    CHECK(zero.advantage_ns() == T{.A = 0.0, .B = 0.0});
  }
  SECTION("nonzero A")
  { //
    stats s = zero;
    s.faster_count.A = 2;
    s.advantage_total_ns.A = 5;
    CHECK(s.advantage_ns() == T{.A = 2.5, .B = 0.0});
  }
  SECTION("nonzero B")
  { //
    stats s = zero;
    s.faster_count.B = 3;
    s.advantage_total_ns.B = 4.5;
    CHECK(s.advantage_ns() == T{.A = 0, .B = 1.5});
  }
}

struct Logger {
  std::vector<std::string> log;

  void append(std::string line) { log.push_back(std::move(line)); }
  auto fn() -> stats::error_callback_t
  {
    return [this](std::string line) { this->append(std::move(line)); };
  }

  static Logger const empty;

  [[nodiscard]] auto operator==(Logger const &other) const noexcept -> bool = default;
};

Logger const Logger::empty = {};

inline auto operator<<(std::ostream &output, Logger const &self) -> std::ostream &
{
  for (auto const &line : self.log) {
    output << line << '\n';
  }
  return output;
}

TEST_CASE("stats calculation from inputs")
{
  stats const zero{.packet_count = {}, .dropped_count = {}, .faster_count = {}, .advantage_total_ns = {}};

  SECTION("empty inputs")
  {
    Logger logger;
    CHECK(stats::make(MockInputs({.A = {}, .B = {}}), logger.fn()) == zero);
    CHECK(logger == Logger::empty);
  }

  SECTION("incomplete packets")
  {
    {
      Logger logger;
      CHECK(stats::make(MockInputs({.A = {{}, {}}, .B = {}}), logger.fn()) == zero);
      CHECK(logger == Logger{{{"0,not enough data"}, {"0,not enough data"}}});
    }
    {
      Logger logger;
      CHECK(stats::make(MockInputs({.A = {}, .B = {{}, {}}}), logger.fn()) == zero);
      CHECK(logger == Logger{{{"1,not enough data"}, {"1,not enough data"}}});
    }
    {
      Logger logger;
      CHECK(stats::make(MockInputs({.A = {{}}, .B = {{}}}), logger.fn()) == zero);
      CHECK(logger == Logger{{{"0,not enough data"}, {"1,not enough data"}}});
    }
  }

  SECTION("bad packets")
  {
    packet_t example = example_packet;
    REQUIRE(set_ip_protocol(IPPROTO_TCP, example));

    SECTION("channel A")
    {
      {
        Logger logger;
        CHECK(stats::make(MockInputs({.A = {example}, .B = {}}), logger.fn()) == zero);
        CHECK(logger == Logger{{{"0,not UDP"}}});
      }

      {
        Logger logger;
        CHECK(stats::make(MockInputs({.A = {example}, .B = {{}}}), logger.fn()) == zero);
        CHECK(logger == Logger{{{"0,not UDP"}, {"1,not enough data"}}});
      }
    }

    SECTION("channel B")
    {
      {
        Logger logger;
        CHECK(stats::make(MockInputs({.A = {}, .B = {example}}), logger.fn()) == zero);
        CHECK(logger == Logger{{{"1,not UDP"}}});
      }

      {
        Logger logger;
        CHECK(stats::make(MockInputs({.A = {{}}, .B = {example}}), logger.fn()) == zero);
        CHECK(logger == Logger{{{"0,not enough data"}, {"1,not UDP"}}});
      }
    }
  }

  SECTION("some packets with data")
  {
    SECTION("one packet in one channel only")
    {
      SECTION("channel A")
      {
        Logger logger;
        stats const expected = //
            {.packet_count{.A = 1, .B = 0},
             .dropped_count{.A = 0, .B = 0},
             .faster_count{.A = 0, .B = 0},
             .advantage_total_ns{.A = 0.0, .B = 0}};
        CHECK(stats::make(MockInputs({.A = {example_packet}, .B = {}}), logger.fn()) == expected);
        CHECK(logger == Logger::empty);
      }

      SECTION("channel B")
      {
        Logger logger;
        stats const expected = //
            {.packet_count{.A = 0, .B = 1},
             .dropped_count{.A = 0, .B = 0},
             .faster_count{.A = 0, .B = 0},
             .advantage_total_ns{.A = 0.0, .B = 0}};
        CHECK(stats::make(MockInputs({.A = {}, .B = {example_packet}}), logger.fn()) == expected);
        CHECK(logger == Logger::empty);
      }
    }

    using namespace std::chrono_literals;
    SECTION("one synced packet in each channel")
    {
      packet_t exampleA = example_packet;
      packet_t exampleB = exampleA;
      auto const timestampA = get_timestamp(exampleA);
      REQUIRE(timestampA.has_value());
      set_timestamp(*timestampA + 40ns, exampleB);
      Logger logger;
      stats const expected = //
          {.packet_count{.A = 1, .B = 1},
           .dropped_count{.A = 0, .B = 0},
           .faster_count{.A = 1, .B = 0},
           .advantage_total_ns{.A = 40.0, .B = 0}};
      CHECK(stats::make(MockInputs({.A = {exampleA}, .B = {exampleB}}), logger.fn()) == expected);
      CHECK(logger == Logger::empty);
    }

    SECTION("different number of packets")
    {
      auto const sequence = get_sequence(example_packet);
      REQUIRE(sequence.has_value());
      auto const timestamp = get_timestamp(example_packet);
      REQUIRE(timestamp.has_value());

      packet_t exampleA1 = example_packet;
      packet_t exampleB1 = exampleA1;
      set_timestamp(*timestamp - 2us, exampleB1);
      packet_t exampleB2 = exampleB1;
      set_timestamp(*timestamp + 300us, exampleB2);
      set_sequence(*sequence + 2, exampleB2);

      Logger logger;
      stats const expected = //
          {.packet_count{.A = 1, .B = 2},
           .dropped_count{.A = 0, .B = 0},
           .faster_count{.A = 0, .B = 1},
           .advantage_total_ns{.A = 0, .B = 2000.0}};
      CHECK(stats::make(MockInputs({.A = {exampleA1}, .B = {exampleB1, exampleB2}}), logger.fn()) == expected);
      CHECK(logger == Logger::empty);
    }

    SECTION("two synced packets")
    {
      auto const sequence = get_sequence(example_packet);
      REQUIRE(sequence.has_value());
      auto const timestamp = get_timestamp(example_packet);
      REQUIRE(timestamp.has_value());

      packet_t exampleA1 = example_packet;
      packet_t exampleA2 = exampleA1;
      set_timestamp(*timestamp + 120us, exampleA2);
      set_sequence(*sequence + 1, exampleA2);
      packet_t exampleB1 = exampleA1;
      set_timestamp(*timestamp - 2us, exampleB1);
      packet_t exampleB2 = exampleB1;
      set_timestamp(*timestamp + 120us, exampleB2);
      set_sequence(*sequence + 1, exampleB2);

      Logger logger;
      stats const expected = //
          {.packet_count{.A = 2, .B = 2},
           .dropped_count{.A = 0, .B = 0},
           .faster_count{.A = 0, .B = 1},
           .advantage_total_ns{.A = 0, .B = 2000.0}};
      CHECK(stats::make(MockInputs({.A = {exampleA1, exampleA2}, .B = {exampleB1, exampleB2}}), logger.fn())
            == expected);
      CHECK(logger == Logger::empty);
    }

    SECTION("one synced packet, one packet drop")
    {
      auto const sequence = get_sequence(example_packet);
      REQUIRE(sequence.has_value());
      auto const timestamp = get_timestamp(example_packet);
      REQUIRE(timestamp.has_value());

      packet_t exampleA1 = example_packet;
      packet_t exampleA2 = exampleA1;
      set_timestamp(*timestamp + 120us, exampleA2);
      set_sequence(*sequence + 1, exampleA2);
      packet_t exampleB1 = exampleA1;
      set_timestamp(*timestamp - 2us, exampleB1);
      packet_t exampleB2 = exampleB1;
      set_timestamp(*timestamp + 300us, exampleB2);
      set_sequence(*sequence + 2, exampleB2);

      Logger logger;
      stats const expected = //
          {.packet_count{.A = 2, .B = 2},
           .dropped_count{.A = 0, .B = 1},
           .faster_count{.A = 0, .B = 1},
           .advantage_total_ns{.A = 0, .B = 2000.0}};
      CHECK(stats::make(MockInputs({.A = {exampleA1, exampleA2}, .B = {exampleB1, exampleB2}}), logger.fn())
            == expected);
      CHECK(logger == Logger::empty);
    }
  }
}
