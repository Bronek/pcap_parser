#include "stats.hpp"
#include "functional.hpp"
#include "inputs.hpp"
#include "pair.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

struct state_t {
  using duration = std::chrono::system_clock::duration;
  static_assert(std::same_as<duration, std::chrono::nanoseconds>);
  using time_point = std::chrono::system_clock::time_point;

  time_point timestamp = {};
  uint32_t sequence = 0;
  bool read_next = true;
  pair_select const which;
};

void packet_dump(std::ostream &out, Inputs::data_t const &data)
{
  std::ostringstream ss;
  ss << '{'               //
     << std::setfill('0') //
     << std::hex;
  for (unsigned int i = 0; i < data.size(); ++i) {
    ss << "0x" << std::setw(2) << static_cast<unsigned int>(data[i]);
    if (i + 1 < data.size()) {
      ss << ", ";
    }
  }
  ss << '}';
  out << ss.str();
};

enum class packet_error : int { unknown = 0, not_enough_data, not_ipv4, not_udp, bad_ip_header, bad_udp_header };
auto operator<<(std::ostream &out, packet_error e) -> std::ostream &
{
  switch (e) {
  case packet_error::not_enough_data:
    out << "not_enough_data";
    break;
  case packet_error::not_ipv4:
    out << "not_ipv4";
    break;
  case packet_error::not_udp:
    out << "not_udp";
    break;
  case packet_error::bad_ip_header:
    out << "bad_ip_header";
    break;
  case packet_error::bad_udp_header:
    out << "bad_udp_header";
    break;
  default:
    out << "unknown";
  }
  return out;
}

template <typename T> using expected = std::expected<T, packet_error>;

void update(state_t &state, Inputs::data_t data, stats::error_callback_t &log)
{
  constexpr std::size_t ethernet_header_length = 14;
  constexpr std::size_t minimum_ip_header_length = 20;
  constexpr std::size_t maximum_ip_header_length = 60;
  constexpr std::size_t ip_protocol_offset = 9;
  constexpr std::size_t minimum_payload_length = sizeof(udphdr) + 4;
  constexpr std::size_t metamako_trailer_length = 20;
  constexpr std::size_t metamako_seconds_offset = 8;
  constexpr std::size_t metamako_nanoseconds_offset = 12;

  using packet_t = Inputs::data_t;
  struct udp_t final {
    packet_t packet;
    int ip_header_len;
  };
  struct payload_t final {
    udp_t packet;
    int payload_len;
  };

  constexpr auto minimum_frame_length =    //
      ethernet_header_length               // Ethernet frame
      + minimum_ip_header_length           // IPv4 header (variable size, just check minimum here)
      + minimum_payload_length             // UDP header + 4 bytes sequence number
      + metamako_trailer_length;           // Metamako trailer with timestamp
  expected<packet_t> const packet =        //
      (data.size() < minimum_frame_length) //
          ? expected<packet_t>(std::unexpect, packet_error::not_enough_data)
          : data;
  // TODO: clean up C-style casts and type punning.
  packet //
      | and_then([](packet_t const &data) noexcept -> expected<packet_t> {
          auto const *eth_header = (::ether_header const *)data.data();
          if (::ntohs(eth_header->ether_type) != ETHERTYPE_IP) {
            return expected<packet_t>(std::unexpect, packet_error::not_ipv4);
          }
          return data;
        })
      | and_then([](packet_t const &data) noexcept -> expected<udp_t> {
          auto const *ip_header = (data.data() + ethernet_header_length);
          int const ip_header_length = (*ip_header & 0x0F) * 4;
          if (*(ip_header + ip_protocol_offset) != IPPROTO_UDP) {
            return expected<udp_t>(std::unexpect, packet_error::not_udp);
          }
          if (ip_header_length < static_cast<int>(minimum_ip_header_length)
              || ip_header_length > static_cast<int>(maximum_ip_header_length)
              || data.size()
                     < ethernet_header_length + ip_header_length + minimum_payload_length + metamako_trailer_length) {
            return expected<udp_t>(std::unexpect, packet_error::bad_ip_header);
          }

          return udp_t{.packet = data, .ip_header_len = ip_header_length};
        })
      | and_then([](udp_t const &d) noexcept -> expected<payload_t> {
          auto const *udp_header = (::udphdr const *)(d.packet.data() + ethernet_header_length + d.ip_header_len);
          auto const payload_length = ::ntohs(udp_header->len);
          if (payload_length < minimum_payload_length
              || d.packet.size()
                     != ethernet_header_length + d.ip_header_len + payload_length + metamako_trailer_length) {
            return expected<payload_t>(std::unexpect, packet_error::bad_udp_header);
          }
          return payload_t{.packet = d, .payload_len = payload_length};
        })
      | transform([&state](payload_t const &d) -> void {
          auto const *ip_payload = d.packet.packet.data() + ethernet_header_length + d.packet.ip_header_len;
          auto const sequence = *((uint32_t const *)(ip_payload + sizeof(udphdr)));
          auto const seconds = ::ntohl(*((uint32_t const *)(ip_payload + d.payload_len + metamako_seconds_offset)));
          auto const nanoseconds
              = ::ntohl(*((uint32_t const *)(ip_payload + d.payload_len + metamako_nanoseconds_offset)));
          state.sequence = sequence;
          state.timestamp = state_t::time_point(std::chrono::seconds(seconds) + std::chrono::nanoseconds(nanoseconds));
          // std::cout << (int)state.which << ',' << state.sequence << ',' << state.timestamp.time_since_epoch().count()
          //           << '\n';
          // packet_dump(std::cout, d.packet.packet);
          // std::cout << std::endl;
        })
      | or_else([&state, &log](packet_error e) -> expected<void> {
          if (log) {
            std::ostringstream ss;
            ss << (int)state.which << ',' << e;
            // packet_dump(ss, data);
            log(ss.str());
          }

          return {};
        })
      | discard();
}

auto stats::make(Inputs &inputs, error_callback_t log) -> stats
{
  stats ret{.packet_count = {}, .dropped_count = {}, .faster_count = {}, .advantage_total_ns = {}};

  pair<state_t> state = {.A = {.which = pair_select::A}, .B = {.which = pair_select::B}};
  while (true) {
    auto const state_old = state;
    bool const read_a = state.A.read_next && inputs.next(pair_select::A, [&state, &log](Inputs::data_t const &data) { //
      update(state.A, data, log);
    });
    bool const read_b = state.B.read_next && inputs.next(pair_select::B, [&state, &log](Inputs::data_t const &data) { //
      update(state.B, data, log);
    });
    if (!read_a && !read_b) {
      break;
    }

    // NOTE: out-of-order & late packets count as dropped, since they failed to arrive when they were needed
    bool const updated_a = state_old.A.sequence < state.A.sequence;
    if (updated_a) {
      ret.packet_count.A += 1;
    } else if (state_old.A.sequence != state.A.sequence) {
      if (log) {
        log("0,out_of_sequence");
      }
    }

    bool const updated_b = state_old.B.sequence < state.B.sequence;
    if (updated_b) {
      ret.packet_count.B += 1;
    } else if (state_old.B.sequence != state.B.sequence) {
      if (log) {
        log("1,out_of_sequence");
      }
    }

    if (updated_a && updated_b) {
      if (state.A.sequence < state.B.sequence) [[unlikely]] {
        ret.dropped_count.B += 1;
      } else if (state.B.sequence < state.A.sequence) [[unlikely]] {
        ret.dropped_count.A += 1;
      } else { // state.B.sequence == state.A.sequence
        if (state.A.timestamp < state.B.timestamp) {
          ret.faster_count.A += 1;
          ret.advantage_total_ns.A += //
              static_cast<double>(state.B.timestamp.time_since_epoch().count()
                                  - state.A.timestamp.time_since_epoch().count());
        } else if (state.B.timestamp < state.A.timestamp) {
          ret.faster_count.B += 1;
          ret.advantage_total_ns.B += //
              static_cast<double>(state.A.timestamp.time_since_epoch().count()
                                  - state.B.timestamp.time_since_epoch().count());
        }
        // else neither channel has advantage, that's unusual but possible
      }
    }

    state.A.read_next = (state.A.sequence <= state.B.sequence);
    state.B.read_next = (state.B.sequence <= state.A.sequence);
  }

  return ret;
}
