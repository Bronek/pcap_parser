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
      callback(pkt_header->caplen, pkt_header->len, data);
      return true;
    }
    return false;
  }

  auto next_a(std::move_only_function<callback_t> callback) -> bool override
  {
    return next_(A_.get(), std::move(callback));
  }
  auto next_b(std::move_only_function<callback_t> callback) -> bool override
  {
    return next_(B_.get(), std::move(callback));
  }

  pcap_handle A_;
  pcap_handle B_;
};

#endif // LIB_PCAP_INPUTS
