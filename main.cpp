#include "lib/find_inputs.hpp"
#include "lib/functional.hpp"
#include "lib/pcap_inputs.hpp"
#include "lib/sort_channels.hpp"
#include "lib/stats.hpp"

#include <expected>
#include <iostream>
#include <string>

// NOTE: exception and fail NOT used outside of main.cpp
struct exception final {
  int code;
};

[[noreturn]] void fail(int code, auto &&...args)
{
  ((std::cerr << args), ...);
  std::cerr << '\n';
  throw exception(code);
}

auto main(int argc, char const **argv) -> int
{
  try {
    if (argc != 2) {
      fail(13, "received ", argc - 1, " parameters but expected 1");
    }

    find_inputs(argv[1])                               // untested (direct filesystem calls)
        | and_then(&sort_channels)                     // tested
        | and_then(&PcapInputs::make)                  // untested (direct libpcap calls)
        | transform([](PcapInputs &&inputs) -> stats { //
            return stats::make(inputs);
          })
        | transform([](stats const &result) -> void {
            std::cout << result << std::endl; //
          })
        | or_else([](error const &err) -> std::expected<void, error> {
            fail(1, err); // NOTE: noreturn
          })
        | discard();
  } catch (exception e) {
    return e.code;
  } catch (std::exception const &e) {
    std::cerr << e.what() << '\n';
    return 2;
  }

  return 0;
}
