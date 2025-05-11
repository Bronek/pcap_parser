#include "lib/find_inputs.hpp"
#include "lib/functional.hpp"
#include "lib/pcap_inputs.hpp"
#include "lib/sort_channels.hpp"
#include "lib/stats.hpp"

#include <expected>
#include <iostream>
#include <string>

auto main(int argc, char const **argv) -> int
try {
  auto path = [&]() -> std::expected<std::string, error> {
    if (argc != 2) {
      return error::make(error::main, "received ", argc - 1, " parameters but expected 1");
    }
    return {argv[1]};
  }();

  return (std::move(path)              //
          | and_then(find_inputs)      // untested (direct filesystem calls)
          | and_then(sort_channels)    // tested in sort_channels.cpp
          | and_then(PcapInputs::make) // untested (direct libpcap calls)
          | transform(stats::make)     // tested in stats.cpp (minimal) and packet.cpp
          | transform([](stats const &result) -> int {
              std::cout << result << std::endl;
              return 0;
            })
          | or_else([](error const &err) -> std::expected<int, error> {
              std::cerr << err << std::endl;
              return err.code();
            }))
      .value();
} catch (std::exception const &e) {
  std::cerr << e.what() << '\n';
  return 3;
}
