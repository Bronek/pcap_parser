#include "find_inputs.hpp"

#include <filesystem>

// TODO: write a std::filesystem wrapper to make this testable.
// TODO: make it more forgiving (e.g. ignore stray directories)
auto find_inputs(std::string const &strpath) -> std::expected<input_files, error>
{
  namespace fs = std::filesystem;
  fs::path const path(strpath);
  if (!fs::exists(path)) {
    return error::make("path ", path.c_str(), " does not exist");
  }
  if (fs::status(path).type() != fs::file_type::directory) {
    return error::make("path ", path.c_str(), " is not a directory");
  }

  input_files result;
  int i = 0;
  for (auto const &direntry : fs::directory_iterator(path)) {
    if (i > 1) {
      return error::make("too many files in directory: ", path.c_str());
    }
    result[i++] = direntry.path().string();
  }
  if (i != 2) {
    return error::make("too few files in directory: ", path.c_str());
  }
  if (fs::status(result[0]).type() != fs::file_type::regular
      || fs::status(result[1]).type() != fs::file_type::regular) {
    return error::make("an input is not a regular file: ", result[0], ", ", result[1]);
  }

  return result;
}
