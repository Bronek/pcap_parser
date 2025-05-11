#ifndef LIB_INPUTS
#define LIB_INPUTS

#include "pair.hpp"

#include <functional>
#include <span>
#include <utility>

// NOTE: I am using CamelCase for naming of polymorphic classes, like here
//
// The reason why this class is polymorphic is to provide a customization point for unit tests.
struct Inputs {
  auto next(pair_select which, auto &&callback) -> bool
  {
    switch (which) {
    case pair_select::A:
      return this->next_a(std::forward<decltype(callback)>(callback));
    case pair_select::B:
      return this->next_b(std::forward<decltype(callback)>(callback));
    default:
      std::unreachable();
    }
  }

  using data_t = std::span<unsigned char const>;
  using data_callback_t = std::move_only_function<void(data_t)>;

private:
  virtual auto next_a(data_callback_t) -> bool = 0;
  virtual auto next_b(data_callback_t) -> bool = 0;
};

#endif // LIB_ERROR
