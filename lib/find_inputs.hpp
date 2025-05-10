#ifndef LIB_FIND_INPUTS
#define LIB_FIND_INPUTS

#include "error.hpp"

#include <array>
#include <expected>
#include <string>

using input_files = std::array<std::string, 2>;

// Extract two filenames from a given directory.
auto find_inputs(std::string const &strpath) -> std::expected<input_files, error>;

#endif // LIB_FIND_INPUTS
