#ifndef LIB_INPUTS
#define LIB_INPUTS

#include "pair.hpp"

#include <concepts>
#include <cstdint>
#include <functional>
#include <utility>

// NOTE: I am using CamelCase for naming of polymorphic classes, like here
//
// The reason why this class is polymorphic is to provide a customization point for unit tests. Ideally this
// customization point would be for pcap functions, but I do not have the time to learn pcap well enough and write an
// abstraction for these.
struct Inputs {
  auto next(pair_select which, auto &&fn) -> bool
  {
    switch (which) {
    case pair_select::A:
      return this->next_a(fn);
    case pair_select::B:
      return this->next_b(fn);
    default:
      std::unreachable();
    }
  }

  using callback_t = void(int32_t, int32_t, unsigned char const *);

  // Niebloid conversion from other types (e.g. derived) to this class. Use as a bridge from polymorphic class hierarchy
  // to functional style sequence, where left side is a concrete type, while right consumes reference to this class.
  // NOTE: Not a regular function because argument type deduction does not (quite) work with pointer to a function.
  static constexpr struct {
    [[nodiscard]] auto operator()(auto &&source) const
        noexcept(noexcept(static_cast<Inputs &>(source))) -> std::reference_wrapper<Inputs>
      requires std::convertible_to<decltype(source) &, Inputs &>
    {
      return std::ref(static_cast<Inputs &>(source)); // NOTE: forced lvalue
    }
  } cast = {};

private:
  virtual auto next_a(std::move_only_function<callback_t>) -> bool = 0;
  virtual auto next_b(std::move_only_function<callback_t>) -> bool = 0;
};

#endif // LIB_ERROR
