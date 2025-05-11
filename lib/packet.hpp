#ifndef LIB_PACKET
#define LIB_PACKET

#include "error.hpp"

#include <chrono>
#include <ostream>
#include <span>

namespace packet {

using data_t = std::span<unsigned char const>;

void print(std::ostream &out, data_t const &data);

struct properties final {
  using duration = std::chrono::system_clock::duration;
  static_assert(std::same_as<duration, std::chrono::nanoseconds>);
  using time_point = std::chrono::system_clock::time_point;

  time_point timestamp = {};
  uint32_t sequence = 0;

  [[nodiscard]] constexpr bool operator==(properties const &other) const noexcept = default;
};

constexpr inline struct parse_t final {
  [[nodiscard]] auto operator()(data_t const &) const -> std::expected<properties, error>;
} parse;

} // namespace packet

#endif // LIB_PACKET
