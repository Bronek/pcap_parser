#ifndef LIB_PAIR
#define LIB_PAIR

#include <ostream>

// NOTE: I am using snake_case for simple types, e.g. non-polymorphic, aggregate etc.
enum class pair_select : bool { A, B };

// Representation of a A/B pair of some type
template <typename T> struct pair final {
  T A;
  T B;

  [[nodiscard]] constexpr auto operator==(pair const &) const noexcept -> bool = default;
};

template <typename T>
auto operator<<(std::ostream &output, pair<T> const &self) -> std::ostream &
{ //
  return (output << "(A=" << self.A << ", B=" << self.B << ')');
}

#endif // LIB_PAIR
