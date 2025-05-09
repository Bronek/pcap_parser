#include "lib/error.hpp"
#include "lib/inputs.hpp"
#include "lib/stats.hpp"

#include <expected>
#include <filesystem>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>

using input_files = std::array<std::string, 2>;

// TODO: write a std::filesystem wrapper to make this testable.
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

// NOTE: Not used outside of main.cpp
struct exception final {
  int code;
};

[[noreturn]] void fail(int code, auto &&...args)
{
  ((std::cerr << args), ...);
  std::cerr << '\n';
  throw exception(code);
}

namespace detail {
template <typename> constexpr bool _is_some_expected = false;
template <typename T, typename E> constexpr bool _is_some_expected<std::expected<T, E> &> = true;
template <typename T, typename E> constexpr bool _is_some_expected<std::expected<T, E> const &> = true;
} // namespace detail
template <typename T>
concept some_expected = detail::_is_some_expected<T &>;

auto operator|(some_expected auto &&monad, auto &&fn) //
    -> std::invoke_result_t<decltype(fn), decltype(std::forward<decltype(monad)>(monad).value())>
{
  if (monad) {
    return std::invoke(fn, std::forward<decltype(monad)>(monad).value());
  }
  fail(1, monad.error());
  std::unreachable();
}

auto main(int argc, char const **argv) -> int
{
  try {
    if (argc != 2) {
      fail(13, "received ", argc - 1, " parameters but expected 1");
    }

    find_inputs(argv[1]) | sort | PcapInputs::make | Inputs::cast | stats::make |
        [](stats const &result) { std::cout << result << std::endl; };
  } catch (exception e) {
    return e.code;
  } catch (std::exception const &e) {
    std::cerr << e.what() << '\n';
    return 2;
  }

  return 0;
}
