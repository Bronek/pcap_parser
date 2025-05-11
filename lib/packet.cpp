#include "packet.hpp"
#include "functional.hpp"

#include <iomanip>
#include <sstream>

#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

void packet::print(std::ostream &out, data_t const &data)
{
  std::ostringstream ss;
  ss << '{'               //
     << std::setfill('0') //
     << std::hex;
  for (unsigned int i = 0; i < data.size(); ++i) {
    ss << "0x" << std::setw(2) << static_cast<unsigned int>(data[i]);
    if (i + 1 < data.size()) {
      ss << ',';
      if ((i + 1) % 8 == 0) {
        ss << '\n';
      } else {
        ss << ' ';
      }
    }
  }
  ss << '}';
  out << ss.str();
}

namespace {

constexpr std::size_t ethernet_header_length = 14;
constexpr std::size_t minimum_ip_header_length = 20;
constexpr std::size_t maximum_ip_header_length = 60;
constexpr std::size_t ip_protocol_offset = 9;
constexpr std::size_t minimum_payload_length = sizeof(::udphdr) + 4;
constexpr std::size_t metamako_trailer_length = 20;
constexpr std::size_t metamako_seconds_offset = 8;
constexpr std::size_t metamako_nanoseconds_offset = 12;

struct udp_t {
  int ip_header_len;
};
struct payload_t : udp_t {
  int payload_len;
};

} // namespace

auto packet::parse_t::operator()(data_t const &data) const -> std::expected<properties, error>
{
  // TODO: clean up C-style casts and type punning.
  return std::expected<void, error>() //
         | and_then([&data]() noexcept -> std::expected<void, error> {
             // Is this Ethernet frame an actual IPv4 packet, and does it have the minimum lenght for one ?
             constexpr auto minimum_frame_length = //
                 ethernet_header_length            // Ethernet frame
                 + minimum_ip_header_length        // IPv4 header (variable size, just check minimum here)
                 + minimum_payload_length          // UDP header + 4 bytes sequence number
                 + metamako_trailer_length;        // Metamako trailer with timestamp
             if (data.size() < minimum_frame_length) {
               return error::make(error::packet_parse, "not enough data");
             }

             auto const *eth_header = (::ether_header const *)data.data();
             if (::ntohs(eth_header->ether_type) != ETHERTYPE_IP) {
               return error::make(error::packet_parse, "not IPv4");
             }

             return {};
           })
         | and_then([&data]() noexcept -> std::expected<udp_t, error> {
             // Is this an UDP packet, does it have the required size and what is its IP header length ?
             auto const *ip_header = (data.data() + ethernet_header_length);
             int const ip_header_length = (*ip_header & 0x0F) * 4;
             if (*(ip_header + ip_protocol_offset) != IPPROTO_UDP) {
               return error::make(error::packet_parse, "not UDP");
             }

             if (ip_header_length < static_cast<int>(minimum_ip_header_length)
                 || ip_header_length > static_cast<int>(maximum_ip_header_length)
                 || data.size() < ethernet_header_length + ip_header_length + minimum_payload_length
                                      + metamako_trailer_length) {
               return error::make(error::packet_parse, "bad IP header");
             }

             return udp_t{ip_header_length};
           })
         | and_then([&data](udp_t udp) noexcept -> std::expected<payload_t, error> {
             // Extract payload size and validate frame size
             auto const *udp_header = (::udphdr const *)(data.data() + ethernet_header_length + udp.ip_header_len);
             auto const payload_length = ::ntohs(udp_header->len);
             if (payload_length < minimum_payload_length
                 || data.size()
                        != ethernet_header_length + udp.ip_header_len + payload_length + metamako_trailer_length) {
               return error::make(error::packet_parse, "bad UDP header");
             }

             return payload_t{udp, payload_length};
           })
         | transform([&data](payload_t payload) -> packet::properties {
             // Extract packet properties, i.e. sequence and Metamako timestamp
             auto const *ip_payload = data.data() + ethernet_header_length + payload.ip_header_len;
             auto const sequence = *((uint32_t const *)(ip_payload + sizeof(::udphdr)));
             auto const seconds
                 = ::ntohl(*((uint32_t const *)(ip_payload + payload.payload_len + metamako_seconds_offset)));
             auto const nanoseconds
                 = ::ntohl(*((uint32_t const *)(ip_payload + payload.payload_len + metamako_nanoseconds_offset)));
             auto const timestamp = packet::properties::time_point(std::chrono::seconds(seconds)
                                                                   + std::chrono::nanoseconds(nanoseconds));

             return packet::properties{.timestamp = timestamp, .sequence = sequence};
           });
}
