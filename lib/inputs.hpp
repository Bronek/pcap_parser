#ifndef LIB_INPUTS
#define LIB_INPUTS

#include "error.hpp"
#include "pair.hpp"

#include <array>
#include <concepts>
#include <cstdint>
#include <expected>
#include <functional>
#include <utility>

#include <pcap.h>
#include <pcap/pcap.h>

std::expected<pair<std::string>, error> sort(std::array<std::string, 2> const &files);

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

  // Niebloid conversion from other types (typically derived) to this class, for use in functional sequences
  static constexpr struct {
    [[nodiscard]] auto operator()(auto &&source) const -> std::expected<std::reference_wrapper<Inputs>, std::nullptr_t>
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
