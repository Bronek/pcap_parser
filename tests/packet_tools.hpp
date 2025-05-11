#ifndef TESTS_PACKET_TOOLS
#define TESTS_PACKET_TOOLS

#include <vector>
#include <cstdint>
#include <chrono>
#include <optional>

#include <netinet/in.h>

using packet_t = std::vector<unsigned char>;

static packet_t const example_packet =                                  //
    {0x01, 0x00, 0x5e,           0x00, 0x1f, 0x01, 0x10,          0x0e, //
     0x7e, 0xe7, 0x20,           0x44, 0x08, 0x00, /*IP:*/ 0x45,  0x00,
     0x00, 0x80, 0xb7,           0x5f, 0x40, 0x00, 0x3d,          /*proto:*/ 0x11,
     0xdb, 0xf3, 0xcd,           0xd1, 0xdd, 0x46, 0xe0,          0x00,
     0x1f, 0x01, /*UDP:*/ 0x37,  0xe6, 0x37, 0xe6, /*len:*/ 0x00, 0x10,
     0x00, 0x00, /*data:*/ 0xbd, 0x49, 0xb6, 0x01, 0x00,          0x00,
     0x00, 0x00, /*tail:*/ 0x00, 0x00, 0x00, 0x00, 0x00,          0x00,
     0x00, 0x00, 0x5d,           0x68, 0x06, 0xdc, 0x13,          0x44,
     0xb1, 0x46, 0x00,           0x00, 0x00, 0x00};

inline auto set_ethertype(uint16_t type, packet_t &data) -> bool
{
  if (data.size() > 13UL) {
    (*(uint16_t *)(&data[12])) = ::htons(type);
    return true;
  }
  return false;
}

inline auto set_ip_header_len(uint8_t len, packet_t &data) -> uint8_t
{
  if (data.size() > 14UL) {
    data[14] &= 0xF0;
    data[14] |= (len / 4);
    return (data[14] & 0x0F) * 4;
  }
  return 0;
}

inline auto set_ip_protocol(uint8_t type, packet_t &data) -> bool
{
  if (data.size() > std::size_t(14 + 9)) {
    data[14 + 9] = type;
    return true;
  }
  return false;
}

inline auto set_udp_payload_len(uint16_t len, packet_t &data) -> bool
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

inline auto set_sequence(uint32_t seq, packet_t &data) -> bool
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

inline auto get_sequence(packet_t const &data) -> std::optional<uint32_t>
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

inline auto set_timestamp(std::chrono::system_clock::time_point time, packet_t &data) -> bool
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

inline auto get_timestamp(packet_t const &data) -> std::optional<std::chrono::system_clock::time_point>
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

#endif // TESTS_PACKET_TOOLS
