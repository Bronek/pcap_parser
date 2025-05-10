#ifndef TESTS_MOCK_INPUTS
#define TESTS_MOCK_INPUTS

#include "lib/inputs.hpp"
#include "lib/pair.hpp"

#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <optional>

#include <netinet/in.h>

using packet = std::vector<unsigned char>;

struct MockInputs : Inputs {
  MockInputs(pair<std::initializer_list<packet>> inputs)
      : cursor_{.A = 0, .B = 0}, inputs_{.A = inputs.A, .B = inputs.B}
  {
  }

private:
  static auto next_(std::size_t &next, std::vector<packet> const &input, auto &&callback) -> bool
  {
    if (next < input.size()) {
      auto const &p = input[next++];
      if (p.size() > 0) {
        callback(Inputs::data_t(p.data(), p.size()));
      } else {
        static unsigned char const dummy[8] = {};
        callback(Inputs::data_t(dummy, 0));
      }
      return true;
    }
    return false;
  }
  auto next_a(data_callback_t fn) -> bool override
  { //
    return next_(cursor_.A, inputs_.A, std::move(fn));
  }
  auto next_b(data_callback_t fn) -> bool override
  { //
    return next_(cursor_.B, inputs_.B, std::move(fn));
  }

  pair<std::size_t> cursor_;
  pair<std::vector<packet>> inputs_;
};

#endif // TESTS_MOCK_INPUTS
