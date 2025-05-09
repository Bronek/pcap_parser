#include "stats.hpp"

auto stats::make(Inputs &) -> std::expected<stats, error>
{ //
  // return error::make("not implemented");
  return stats{.packet_count = {}, .orphan_count = {}, .faster_count = {}, .advantage_ns = {}};
}
