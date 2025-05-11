#include "find_inputs.hpp"

#include <filesystem>

// TODO: write a std::filesystem wrapper to make this testable.
auto find_inputs_t::operator()(std::string const &strpath) const -> std::expected<input_files, error>
{
  namespace fs = std::filesystem;
  fs::path const path(strpath);
  if (!fs::exists(path)) {
    return error::make(error::find_inputs, "path does not exist: ", path.c_str());
  }
  if (fs::status(path).type() != fs::file_type::directory) {
    return error::make(error::find_inputs, "path is not a directory: ", path.c_str());
  }

  input_files result;
  int i = 0;
  for (auto const &direntry : fs::directory_iterator(path)) {
    if (!direntry.is_regular_file()) {
      continue; // Ignore subdirectories etc.
    }

    if (i > 1) {
      return error::make(error::find_inputs, "too many files in directory, expected exactly 2: ", path.c_str());
    }
    result[i++] = direntry.path().string();
  }
  if (i != 2) {
    return error::make(error::find_inputs, "too few files in directory, expected exactly 2: ", path.c_str());
  }

  return result;
}
