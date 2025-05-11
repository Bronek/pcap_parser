#ifndef LIB_FIND_INPUTS
#define LIB_FIND_INPUTS

#include "error.hpp"

#include <array>
#include <expected>
#include <string>

using input_files = std::array<std::string, 2>;

// Extract two filenames from a given directory.
constexpr inline struct find_inputs_t final {
  [[nodiscard]] auto operator()(std::string const &strpath) const -> std::expected<input_files, error>;
} find_inputs;

#endif // LIB_FIND_INPUTS
