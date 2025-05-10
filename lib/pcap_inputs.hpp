#ifndef LIB_PCAP_INPUTS
#define LIB_PCAP_INPUTS

#include "error.hpp"
#include "inputs.hpp"

#include <memory>

#include <pcap.h>
#include <pcap/pcap.h>

// TODO: this is untestable, because I do not have the time to write an abstraction for pcap functions
struct PcapInputs final : Inputs {
  [[nodiscard]] static auto make(pair<std::string> filenames) -> std::expected<PcapInputs, error>;

  // noncopyable, but moveable
  PcapInputs(PcapInputs const &) = delete;
  PcapInputs(PcapInputs &&other) : A_(std::move(other.A_)), B_(std::move(other.B_)) {}

private:
  struct pcap_closer final {
    void operator()(pcap_t *p) const noexcept { ::pcap_close(p); }
  };
  using pcap_handle = std::unique_ptr<pcap_t, pcap_closer>;

  explicit PcapInputs(pair<pcap_handle> src) : A_(std::move(src.A)), B_(std::move(src.B)) {}

  static auto next_(pcap_t *input, auto &&callback) -> bool
  {
    pcap_pkthdr *pkt_header = nullptr;
    unsigned char const *data = nullptr;
    // https://www.tcpdump.org/manpages/pcap_next_ex.3pcap.html
    if (auto const ret = ::pcap_next_ex(input, &pkt_header, &data); ret == 1) {
      if (pkt_header->caplen == pkt_header->len) {
        callback(data_t(data, pkt_header->caplen));
      } else {
        // NOTE: Incomplete packet is unlikely to have Metamako trailer with timestamp, hence it is
        // not useful for our purposes. Just report that we had "something" here and move on.
        // NOTE: std::span does not like nullptr even if size is 0; this should be fixed in C++26
        static unsigned char const dummy[8] = {};
        callback(data_t(dummy, 0));
      }
      return true;
    }
    return false;
  }

  auto next_a(data_callback_t callback) -> bool override { return next_(A_.get(), std::move(callback)); }
  auto next_b(data_callback_t callback) -> bool override { return next_(B_.get(), std::move(callback)); }

  pcap_handle A_;
  pcap_handle B_;
};

#endif // LIB_PCAP_INPUTS
