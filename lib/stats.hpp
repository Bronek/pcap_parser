#ifndef LIB_STATS
#define LIB_STATS

#include "error.hpp"
#include "inputs.hpp"
#include "pair.hpp"

#include <expected>
#include <ostream>

// NOTE: I am using snake_case for simple types, e.g. non-polymorphic, aggregate etc.
struct stats {
  pair<std::size_t> packet_count;
  pair<std::size_t> orphan_count;
  pair<std::size_t> faster_count;
  pair<double> advantage_ns;

  static auto make(Inputs &inputs) -> std::expected<stats, error>;
};

inline auto operator<<(std::ostream &output, stats const &self) -> std::ostream &
{
  output << "packet count: " << self.packet_count << '\n' //
         << "orphaned packets count: " << self.orphan_count << '\n'
         << "faster packets count: " << self.faster_count << '\n'
         << "average advantage in ns: " << self.advantage_ns;

  return output;
}

#endif // LIB_STATS
