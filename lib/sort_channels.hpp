#ifndef LIB_SORT_CHANNELS
#define LIB_SORT_CHANNELS

#include "error.hpp"
#include "pair.hpp"

#include <array>
#include <expected>

// For given two files, sort them into channel A and channel B, based on their filenames
constexpr inline struct sort_channels_t final {
  [[nodiscard]] auto
  operator()(std::array<std::string, 2> const &files) const -> std::expected<pair<std::string>, error>;
} sort_channels;

#endif // LIB_SORT_CHANNELS
