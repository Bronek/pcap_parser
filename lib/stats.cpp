#include "stats.hpp"
#include "functional.hpp"
#include "inputs.hpp"
#include "packet.hpp"
#include "pair.hpp"

#include <chrono>
#include <expected>
#include <iostream>
#include <sstream>

namespace {

struct state_t final {
  using duration = std::chrono::system_clock::duration;
  static_assert(std::same_as<duration, std::chrono::nanoseconds>);
  using time_point = std::chrono::system_clock::time_point;

  packet::properties last = {};
  bool read_next = true;
  pair_select const which;
};

} // namespace

auto stats::make_t::operator()(Inputs &&inputs, error_callback_t log) const -> stats
{
  using data_t = Inputs::data_t;
  stats ret{.packet_count = {}, .dropped_count = {}, .faster_count = {}, .advantage_total_ns = {}};

  pair<state_t> state = {.A = {.which = pair_select::A}, .B = {.which = pair_select::B}};
  constexpr auto update = [](state_t &state, error_callback_t &log, data_t const &data) -> void {
    packet::parse(data)                              //
        | transform([&state](packet::properties p) { //
            state.last = std::move(p);
          })
        | or_else([&state, &log](error e) -> std::expected<void, error> {
            if (log) {
              std::ostringstream ss;
              ss << (int)state.which << ',' << e;
              // packet::print(ss, data);
              log(ss.str());
            }
            return {};
          })
        | discard();
  };

  while (true) {
    auto const state_old = state;
    bool const read_a = state.A.read_next && inputs.next(pair_select::A, [&](data_t const &data) { //
      update(state.A, log, data);
    });
    bool const read_b = state.B.read_next && inputs.next(pair_select::B, [&](data_t const &data) { //
      update(state.B, log, data);
    });

    if (!read_a && !read_b) {
      break;
    }

    state.A.read_next = (state.A.last.sequence <= state.B.last.sequence);
    state.B.read_next = (state.B.last.sequence <= state.A.last.sequence);

    // NOTE: out-of-order & late packets count as dropped, since they failed to arrive when they were needed
    bool const updated_a = state_old.A.last.sequence < state.A.last.sequence;
    if (updated_a) {
      ret.packet_count.A += 1;
    } else if (state_old.A.last.sequence != state.A.last.sequence) {
      if (log) {
        log("0,out of sequence");
      }
    }

    bool const updated_b = state_old.B.last.sequence < state.B.last.sequence;
    if (updated_b) {
      ret.packet_count.B += 1;
    } else if (state_old.B.last.sequence != state.B.last.sequence) {
      if (log) {
        log("1,out of sequence");
      }
    }

    if (updated_a && updated_b) {
      if (state.A.last.sequence < state.B.last.sequence) [[unlikely]] {
        ret.dropped_count.B += 1;
        continue;
      }
      if (state.B.last.sequence < state.A.last.sequence) [[unlikely]] {
        ret.dropped_count.A += 1;
        continue;
      }

      // state.B.last.sequence == state.A.last.sequence
      if (state.A.last.timestamp < state.B.last.timestamp) {
        ret.faster_count.A += 1;
        ret.advantage_total_ns.A += //
            static_cast<double>(state.B.last.timestamp.time_since_epoch().count()
                                - state.A.last.timestamp.time_since_epoch().count());
      } else if (state.B.last.timestamp < state.A.last.timestamp) {
        ret.faster_count.B += 1;
        ret.advantage_total_ns.B += //
            static_cast<double>(state.A.last.timestamp.time_since_epoch().count()
                                - state.B.last.timestamp.time_since_epoch().count());
      }
      // else neither channel has advantage, that's unusual but possible
    }
  }

  return ret;
}
