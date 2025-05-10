#ifndef LIB_SORT_CHANNELS
#define LIB_SORT_CHANNELS

#include "error.hpp"
#include "pair.hpp"

#include <array>
#include <expected>

// For given two files, sort them into channel A and channel B, based on their filenames
auto sort_channels(std::array<std::string, 2> const &files) -> std::expected<pair<std::string>, error>;

#endif // LIB_SORT_CHANNELS
