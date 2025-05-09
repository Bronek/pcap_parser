#ifndef LIB_ERROR
#define LIB_ERROR

#include <expected>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

// NOTE: I am using snake_case for simple types, e.g. non-polymorphic, aggregate etc.
struct error {
  error(auto&& src) requires std::convertible_to<decltype(src), std::string> : what_(std::forward<decltype(src)>(src))
  {
  }

  error(auto &&...args)
  {
    std::ostringstream ss;
    ((ss << args), ...);
    what_ = ss.str();
  }

  [[nodiscard]] auto what() const noexcept -> std::string const & { return what_; }

  [[nodiscard]] static auto make(auto &&...args) -> std::unexpected<error>
  {
    return std::unexpected<error>(std::in_place, std::forward<decltype(args)>(args)...);
  }

  [[nodiscard]] auto operator==(error const &other) const noexcept -> bool = default;

private:
  std::string what_;
};

inline auto operator<<(std::ostream &output, error const &self) -> std::ostream & { return (output << self.what()); }

#endif // LIB_ERROR
