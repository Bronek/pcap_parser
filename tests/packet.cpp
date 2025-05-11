#include "packet_tools.hpp"

#include <catch2/catch_all.hpp>

#include <net/ethernet.h>
#include <netinet/in.h>

#include "lib/packet.hpp"

TEST_CASE("packet parsing")
{
  SECTION("not IPv4")
  {
    packet_t example = example_packet;
    REQUIRE(set_ethertype(ETHERTYPE_ARP, example));
    auto const ret = packet::parse(example);
    CHECK(not ret.has_value());
    CHECK(ret.error() == error(error::packet_parse, "not IPv4"));
  }

  SECTION("not UDP")
  {
    packet_t example = example_packet;
    REQUIRE(set_ip_protocol(IPPROTO_TCP, example));
    auto const ret = packet::parse(example);
    CHECK(not ret.has_value());
    CHECK(ret.error() == error(error::packet_parse, "not UDP"));
  }

  SECTION("too small IP header len")
  {
    packet_t example = example_packet;
    REQUIRE(set_ip_header_len(8, example));
    auto const ret = packet::parse(example);
    CHECK(not ret.has_value());
    CHECK(ret.error() == error(error::packet_parse, "bad IP header"));
  }

  SECTION("too large IP header len")
  {
    packet_t example = example_packet;
    example.resize(example.size() + 80);
    REQUIRE(set_ip_header_len(80, example));
    auto const ret = packet::parse(example);
    CHECK(not ret.has_value());
    CHECK(ret.error() == error(error::packet_parse, "bad IP header"));
  }

  SECTION("bad UDP payload len")
  {
    packet_t example = example_packet;
    REQUIRE(set_udp_payload_len(80, example));
    auto const ret = packet::parse(example);
    CHECK(not ret.has_value());
    CHECK(ret.error() == error(error::packet_parse, "bad UDP header"));
  }

  SECTION("happy path")
  {
    packet_t example = example_packet;
    auto const sequence = get_sequence(example);
    REQUIRE(sequence.has_value());
    auto const timestamp = get_timestamp(example);
    REQUIRE(timestamp.has_value());
    auto const ret = packet::parse(example);
    CHECK(ret.has_value());
    CHECK(ret.value() == packet::properties{.timestamp = *timestamp, .sequence = *sequence});
  }
}
