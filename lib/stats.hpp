#ifndef LIB_STATS
#define LIB_STATS

#include "inputs.hpp"
#include "pair.hpp"

#include <functional>
#include <ostream>

// NOTE: I am using snake_case for simple types, e.g. non-polymorphic, aggregate etc.
struct stats {
  pair<std::size_t> packet_count;
  pair<std::size_t> dropped_count;
  pair<std::size_t> faster_count;
  pair<double> advantage_total_ns;

  [[nodiscard]] constexpr auto advantage_ns() const noexcept -> pair<double>
  {
    return {.A = advantage_total_ns.A / static_cast<double>(faster_count.A > 0 ? faster_count.A : 1),
            .B = advantage_total_ns.B / static_cast<double>(faster_count.B > 0 ? faster_count.B : 1)};
  }

  // Produce feed statistics based on network inputs, in pcap format
  using error_callback_t = std::move_only_function<void(std::string)>;
  static constexpr struct make_t final {
    [[nodiscard]] auto operator()(Inputs &&inputs, error_callback_t log = {}) const -> stats;
  } make = {};

  [[nodiscard]] constexpr auto operator==(stats const &other) const noexcept -> bool = default;
};

inline auto operator<<(std::ostream &output, stats const &self) -> std::ostream &
{
  output << "packet count: " << self.packet_count << '\n' //
         << "dropped packets count: " << self.dropped_count << '\n'
         << "faster packets count: " << self.faster_count << '\n'
         << "average advantage in ns: " << self.advantage_ns();

  return output;
}

#endif // LIB_STATS
