#ifndef LIB_ERROR
#define LIB_ERROR

#include <expected>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

// NOTE: I am using snake_case for simple types, e.g. non-polymorphic, aggregate etc.


// Representation of a simple error

// I would *love* if this type was templated by facility, rather than store the
// facility as a data member. This would have, however, given us multiple error
// types rather than one, and mixing multiple error types in C++23 monadic
// operations is tricky. To do this right we need parametric monads, see
// https://github.com/libfn/functional and also type ordering, coming to C++26
struct error final {
  enum facility {
    main = 8,
    find_inputs,
    find_channels,
    open_pcap,
    packet_parse,
  };

  error(facility f, std::string what) : facility_(f), what_(std::move(what)) { check_(); }
  error(facility f, char const *what) : facility_(f), what_(what) { check_(); }

  explicit error(facility f, auto &&...args) : facility_(f)
  {
    check_();

    std::ostringstream ss;
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
  [[nodiscard]] auto code() const noexcept -> int { return facility_; }

  [[nodiscard]] auto operator==(error const &other) const noexcept -> bool = default;

private:
  void check_() const
  {
    if (facility_ <= 0 || facility_ > 127) {
      throw std::logic_error("invalid error facility code");
    }
  }

  int facility_;
  std::string what_;
};

inline auto operator<<(std::ostream &output, error const &self) -> std::ostream &
{ //
  return (output << self.what());
}

#endif // LIB_ERROR
