#ifndef LIB_ERROR
#define LIB_ERROR

#include <expected>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

// NOTE: I am using snake_case for simple types, e.g. non-polymorphic, aggregate etc.

// Representation of a simple error
struct error final {
  error(std::string what) : what_(std::move(what)) {}
  error(char const *what) : what_(what) {}

  explicit error(auto &&first, auto &&...args)
    requires(not std::same_as<std::remove_cvref_t<decltype(first)>, error>)
  {
    std::ostringstream ss;
    ss << first;
    ((ss << args), ...);
    what_ = ss.str();
  }

  error(error const &) = default;
  error(error &&) = default;
  auto operator=(error const &) -> error & = default;
  auto operator=(error &&) -> error & = default;

  // Used to construct the error side of std::expected
  [[nodiscard]] static auto make(auto &&...args) -> std::unexpected<error>
  {
    return std::unexpected<error>(std::in_place, std::forward<decltype(args)>(args)...);
  }

  [[nodiscard]] auto what() const noexcept -> std::string const & { return what_; }

  [[nodiscard]] auto operator==(error const &other) const noexcept -> bool = default;

private:
  std::string what_;
};

inline auto operator<<(std::ostream &output, error const &self) -> std::ostream &
{ //
  return (output << self.what());
}

#endif // LIB_ERROR
