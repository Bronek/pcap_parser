#include <catch2/catch_all.hpp>

#include <cstddef>

#include <net/ethernet.h>
#include <netinet/in.h>

#include "mock_inputs.hpp"

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

static packet const example_packet =                                    //
    {0x01, 0x00, 0x5e,           0x00, 0x1f, 0x01, 0x10,          0x0e, //
     0x7e, 0xe7, 0x20,           0x44, 0x08, 0x00, /*IP:*/ 0x45,  0x00,
     0x00, 0x80, 0xb7,           0x5f, 0x40, 0x00, 0x3d,          /*proto:*/ 0x11,
     0xdb, 0xf3, 0xcd,           0xd1, 0xdd, 0x46, 0xe0,          0x00,
     0x1f, 0x01, /*UDP:*/ 0x37,  0xe6, 0x37, 0xe6, /*len:*/ 0x00, 0x10,
     0x00, 0x00, /*data:*/ 0xbd, 0x49, 0xb6, 0x01, 0x00,          0x00,
     0x00, 0x00, /*tail:*/ 0x00, 0x00, 0x00, 0x00, 0x00,          0x00,
     0x00, 0x00, 0x5d,           0x68, 0x06, 0xdc, 0x13,          0x44,
     0xb1, 0x46, 0x00,           0x00, 0x00, 0x00};

auto set_ethertype(uint16_t type, packet &data) -> bool
{
  if (data.size() > 13UL) {
    (*(uint16_t *)(&data[12])) = ::htons(type);
    return true;
  }
  return false;
}

auto set_ip_header_len(uint8_t len, packet &data) -> uint8_t
{
  if (data.size() > 14UL) {
    data[14] &= 0xF0;
    data[14] |= (len / 4);
    return (data[14] & 0x0F) * 4;
  }
  return 0;
}

auto set_ip_protocol(uint8_t type, packet &data) -> bool
{
  if (data.size() > std::size_t(14 + 9)) {
    data[14 + 9] = type;
    return true;
  }
  return false;
}

auto set_udp_payload_len(uint16_t len, packet &data) -> bool
{
  if (data.size() > 14UL) {
    uint8_t const ip_header_len = (data[14] & 0x0F) * 4;
    if (ip_header_len >= 20 && ip_header_len <= 60 && data.size() > std::size_t(14 + ip_header_len + 5)) {
      (*(uint16_t *)(&data[14 + ip_header_len + 4])) = ::htons(len);
      return true;
    }
  }
  return false;
}

auto set_sequence(uint32_t seq, packet &data) -> bool
{
  if (data.size() > 14UL) {
    uint8_t const ip_header_len = (data[14] & 0x0F) * 4;
    if (data.size() > std::size_t(14 + ip_header_len + 5)) {
      uint16_t const payload_len = ::ntohs(*(uint16_t const *)(&data[14 + ip_header_len + 4]));
      if (payload_len > (8 + 4) && data.size() >= std::size_t(14 + ip_header_len + payload_len)) {
        (*(uint32_t *)(&data[14 + ip_header_len + 8])) = seq;
        return true;
      }
    }
  }
  return false;
}

auto get_sequence(packet const &data) -> std::optional<uint32_t>
{
  if (data.size() > 14UL) {
    uint8_t const ip_header_len = (data[14] & 0x0F) * 4;
    if (data.size() > std::size_t(14 + ip_header_len + 5)) {
      uint16_t const payload_len = ::ntohs(*(uint16_t const *)(&data[14 + ip_header_len + 4]));
      if (payload_len > (8 + 4) && data.size() >= std::size_t(14 + ip_header_len + payload_len)) {
        return {*(uint32_t const *)(&data[14 + ip_header_len + 8])};
      }
    }
  }
  return {};
}

auto set_timestamp(std::chrono::system_clock::time_point time, packet &data) -> bool
{
  if (data.size() > 14UL) {
    uint8_t const ip_header_len = (data[14] & 0x0F) * 4;
    if (data.size() > std::size_t(14 + ip_header_len + 5)) {
      uint16_t const payload_len = ::ntohs(*(uint16_t const *)(&data[14 + ip_header_len + 4]));
      if (payload_len > (8 + 4) && data.size() >= std::size_t(14 + ip_header_len + payload_len + 20)) {
        auto const exact = time.time_since_epoch().count();
        auto const seconds = exact / 1'000'000'000L;
        auto const nanos = (exact - (seconds * 1'000'000'000));
        unsigned char *tail = &data[14 + ip_header_len + payload_len];
        (*(uint32_t *)(&tail[8])) = ::htonl(seconds);
        (*(uint32_t *)(&tail[12])) = ::htonl(nanos);
        return true;
      }
    }
  }
  return false;
}

auto get_timestamp(packet const &data) -> std::optional<std::chrono::system_clock::time_point>
{
  if (data.size() > 14UL) {
    uint8_t const ip_header_len = (data[14] & 0x0F) * 4;
    if (data.size() > std::size_t(14 + ip_header_len + 5)) {
      uint16_t const payload_len = ::ntohs(*(uint16_t const *)(&data[14 + ip_header_len + 4]));
      if (payload_len > (8 + 4) && data.size() >= std::size_t(14 + ip_header_len + payload_len + 20)) {
        unsigned char const *tail = &data[14 + ip_header_len + payload_len];
        auto const seconds = ::ntohl(*(uint32_t const *)(&tail[8]));
        auto const nanos = ::ntohl(*(uint32_t const *)(&tail[12]));
        return {std::chrono::system_clock::time_point{std::chrono::seconds(seconds) + std::chrono::nanoseconds(nanos)}};
      }
    }
  }
  return {};
}

TEST_CASE("stats calculation from inputs")
{
  stats const zero{.packet_count = {}, .dropped_count = {}, .faster_count = {}, .advantage_total_ns = {}};

  SECTION("empty inputs")
  {
    MockInputs inputs({.A = {}, .B = {}});
    Logger logger;
    CHECK(stats::make(inputs, logger.fn()) == zero);
    CHECK(logger == Logger::empty);
  }

  SECTION("incomplete packets")
  {
    {
      MockInputs inputs({.A = {{}, {}}, .B = {}});
      Logger logger;
      CHECK(stats::make(inputs, logger.fn()) == zero);
      CHECK(logger == Logger{{{"0,not_enough_data"}, {"0,not_enough_data"}}});
    }
    {
      MockInputs inputs({.A = {}, .B = {{}, {}}});
      Logger logger;
      CHECK(stats::make(inputs, logger.fn()) == zero);
      CHECK(logger == Logger{{{"1,not_enough_data"}, {"1,not_enough_data"}}});
    }
    {
      MockInputs inputs({.A = {{}}, .B = {{}}});
      Logger logger;
      CHECK(stats::make(inputs, logger.fn()) == zero);
      CHECK(logger == Logger{{{"0,not_enough_data"}, {"1,not_enough_data"}}});
    }
  }

  SECTION("some packets")
  {
    SECTION("not IPv4")
    {
      packet example = example_packet;
      REQUIRE(set_ethertype(ETHERTYPE_ARP, example));
      MockInputs inputs({.A = {example}, .B = {}});
      Logger logger;
      CHECK(stats::make(inputs, logger.fn()) == zero);
      CHECK(logger == Logger{{{"0,not_ipv4"}}});
    }

    SECTION("not UDP")
    {
      packet example = example_packet;
      REQUIRE(set_ip_protocol(IPPROTO_TCP, example));
      MockInputs inputs({.A = {example}, .B = {}});
      Logger logger;
      CHECK(stats::make(inputs, logger.fn()) == zero);
      CHECK(logger == Logger{{{"0,not_udp"}}});
    }

    using namespace std::chrono_literals;
    SECTION("one packet in each channel")
    {
      packet exampleA = example_packet;
      packet exampleB = exampleA;
      auto const timestampA = get_timestamp(exampleA);
      REQUIRE(timestampA.has_value());
      set_timestamp(*timestampA + 40ns, exampleB);
      MockInputs inputs({.A = {exampleA}, .B = {exampleB}});
      Logger logger;
      stats const expected = //
          {.packet_count{.A = 1, .B = 1},
           .dropped_count{.A = 0, .B = 0},
           .faster_count{.A = 1, .B = 0},
           .advantage_total_ns{.A = 40.0, .B = 0}};
      CHECK(stats::make(inputs, logger.fn()) == expected);
      CHECK(logger == Logger::empty);
    }
  }
}
