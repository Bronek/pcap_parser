#ifndef LIB_FUNCTIONAL
#define LIB_FUNCTIONAL

// A minimum subset of functional C++ support tools, written for this project

#include <expected>
#include <functional>
#include <type_traits>

namespace detail {
template <typename> constexpr bool _is_some_expected = false;
template <typename T, typename E> constexpr bool _is_some_expected<std::expected<T, E> &> = true;
template <typename T, typename E> constexpr bool _is_some_expected<std::expected<T, E> const &> = true;
template <typename> constexpr bool _is_some_void_expected = false;
template <typename E> constexpr bool _is_some_void_expected<std::expected<void, E> &> = true;
template <typename E> constexpr bool _is_some_void_expected<std::expected<void, E> const &> = true;
} // namespace detail
template <typename T>
concept some_expected = detail::_is_some_expected<T &>;
template <typename T>
concept some_void_expected = detail::_is_some_void_expected<T &>;

////////////////////////////////////////////////////////////////////////////////
// Functor transform, also known as "fmap"
template <typename Fn> struct transform final {
  Fn function;
};
template <typename Fn> transform(Fn &&fn) -> transform<std::remove_cvref_t<decltype(fn)>>;

// NOTE: clang-20 + libstdc++ do not support std::expected C++23 monadic functions.
// The two overloads below are equivalent to single operator| implemented as:
//   return std::forward<decltype(monad)>(monad).transform(fn.function);

// What does this do ?
// * if expected<...,E> input monad has a value:
//   * invoke fn with value extracted from the input monad
//     * if the input monad is expected<void, E>, invoke fn without parameters (overload below)
//   * if result is not void, return the result wrapped in an expected<...,E> monad
//   * if result is void, return expected<void, E>
// * if expected<...,E> input monad does not have a value:
//   * shortcircuit, returning expected<...,E> with error same as on input

template <typename Fn>
[[nodiscard]] constexpr auto operator|(some_expected auto &&monad, transform<Fn> const &fn) //
    -> std::expected<
        std::invoke_result_t<decltype(fn.function), decltype(std::forward<decltype(monad)>(monad).value())>,
        typename std::remove_cvref_t<decltype(monad)>::error_type>
  requires(not some_void_expected<decltype(monad)>)
          && std::invocable<decltype(fn.function), decltype(std::forward<decltype(monad)>(monad).value())>
{
  using value_t = std::invoke_result_t<decltype(fn.function), decltype(std::forward<decltype(monad)>(monad).value())>;
  using result_t = std::expected<value_t, typename std::remove_cvref_t<decltype(monad)>::error_type>;
  if (monad.has_value()) {
    if constexpr (std::is_void_v<value_t>) {
      std::invoke(fn.function, std::forward<decltype(monad)>(monad).value());
      return result_t{std::in_place};
    } else {
      return result_t{std::in_place, std::invoke(fn.function, std::forward<decltype(monad)>(monad).value())};
    }
  }

  return result_t{std::unexpect, std::forward<decltype(monad)>(monad).error()};
}

template <typename Fn>
[[nodiscard]] constexpr auto operator|(some_void_expected auto &&monad, transform<Fn> const &fn) //
    -> std::expected<std::invoke_result_t<decltype(fn.function)>,
                     typename std::remove_cvref_t<decltype(monad)>::error_type>
  requires std::invocable<decltype(fn.function)>
{
  using value_t = std::invoke_result_t<decltype(fn.function)>;
  using result_t = std::expected<value_t, typename std::remove_cvref_t<decltype(monad)>::error_type>;
  if (monad.has_value()) {
    if constexpr (std::is_void_v<value_t>) {
      std::invoke(fn.function);
      return result_t{std::in_place};
    } else {
      return result_t{std::in_place, std::invoke(fn.function)};
    }
  }

  return result_t{std::unexpect, std::forward<decltype(monad)>(monad).error()};
}

////////////////////////////////////////////////////////////////////////////////
// Functor and_then, also known as "bind" or "flat_map"
template <typename Fn> struct and_then final {
  Fn function;
};
template <typename Fn> and_then(Fn &&fn) -> and_then<std::remove_cvref_t<decltype(fn)>>;

// NOTE: clang-20 + libstdc++ do not support std::expected C++23 monadic functions.
// The two overloads below are equivalent to single operator| implemented as:
//   return std::forward<decltype(monad)>(monad).and_then(fn.function);

// What does this do ?
// * if expected<...,E> input monad has a value:
//   * invoke fn with value extracted from the input monad
//     * if the input monad is expected<void, E>, invoke fn without parameters (overload below)
//   * return result directly
//     * result must be some expected<...,E>
// * if expected<...,E> input monad does not have a value:
//   * shortcircuit, returning expected<...,E> with error same as on input

template <typename Fn>
[[nodiscard]] constexpr auto operator|(some_expected auto &&monad, and_then<Fn> const &fn) //
    -> std::invoke_result_t<decltype(fn.function), decltype(std::forward<decltype(monad)>(monad).value())>
  requires(not some_void_expected<decltype(monad)>)
          && std::invocable<decltype(fn.function), decltype(std::forward<decltype(monad)>(monad).value())>
{
  using result_t = std::invoke_result_t<decltype(fn.function), decltype(std::forward<decltype(monad)>(monad).value())>;
  static_assert(some_expected<result_t>);
  using error_t = std::remove_cvref_t<decltype(monad)>::error_type;
  static_assert(std::is_same_v<typename result_t::error_type, error_t>);
  if (monad.has_value()) {
    return std::invoke(fn.function, std::forward<decltype(monad)>(monad).value());
  }
  return result_t{std::unexpect, std::forward<decltype(monad)>(monad).error()};
}

template <typename Fn>
[[nodiscard]] constexpr auto operator|(some_void_expected auto &&monad, and_then<Fn> const &fn) //
    -> std::invoke_result_t<decltype(fn.function)>
  requires std::invocable<decltype(fn.function)>
{
  using result_t = std::invoke_result_t<decltype(fn.function)>;
  static_assert(some_expected<result_t>);
  using error_t = std::remove_cvref_t<decltype(monad)>::error_type;
  static_assert(std::is_same_v<typename result_t::error_type, error_t>);
  if (monad.has_value()) {
    return std::invoke(fn.function);
  }
  return result_t{std::unexpect, std::forward<decltype(monad)>(monad).error()};
}

////////////////////////////////////////////////////////////////////////////////
// Functor or_else, which is an error-side counterpart of and_then
template <typename Fn> struct or_else final {
  Fn function;
};
template <typename Fn> or_else(Fn &&fn) -> or_else<std::remove_cvref_t<decltype(fn)>>;

// NOTE: clang-20 + libstdc++ do not support std::expected C++23 monadic functions.
// The two overloads below are equivalent to single operator| implemented as:
//   return std::forward<decltype(monad)>(monad).or_else(fn.function);

// What does this do ?
// * if expected<T,...> input monad does not have a value:
//   * invoke fn with error extracted from the input monad
//   * return result directly
//     * result must be some expected<T,...>
//     * void is not a valid error type
// * if expected<T,...> input monad has a value:
//   * shortcircuit, returning expected<T,...> with value same as on input

template <typename Fn>
[[nodiscard]] constexpr auto operator|(some_expected auto &&monad, or_else<Fn> const &fn) //
    -> std::invoke_result_t<decltype(fn.function), decltype(std::forward<decltype(monad)>(monad).error())>
  requires std::invocable<decltype(fn.function), decltype(std::forward<decltype(monad)>(monad).error())>
{
  using result_t = std::invoke_result_t<decltype(fn.function), decltype(std::forward<decltype(monad)>(monad).error())>;
  static_assert(some_expected<result_t>);
  using value_t = std::remove_cvref_t<decltype(monad)>::value_type;
  static_assert(std::is_same_v<typename result_t::value_type, value_t>);
  if (not monad.has_value()) {
    return std::invoke(fn.function, std::forward<decltype(monad)>(monad).error());
  }
  if constexpr (std::is_void_v<typename std::remove_cvref_t<decltype(monad)>::value_type>) {
    return result_t{std::in_place};
  } else {
    return result_t{std::in_place, std::forward<decltype(monad)>(monad).value()};
  }
}

////////////////////////////////////////////////////////////////////////////////
// Functor discard, whose only function is to swallow the input monad

// What does this do ?
// * turns the input monad<...> into a void. Useful for terminating functional sequences !

struct discard final {};
constexpr void operator|(some_expected auto &&, discard const &) noexcept {}

#endif // LIB_FUNCTIONAL
