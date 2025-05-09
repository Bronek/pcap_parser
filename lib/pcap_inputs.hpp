#ifndef LIB_PCAP_INPUTS
#define LIB_PCAP_INPUTS

#include "inputs.hpp"

// TODO: this is untestable, because I do not have the time to write an abstraction for pcap functions
struct PcapInputs final : Inputs {
  [[nodiscard]] static auto make(pair<std::string> filenames) -> std::expected<PcapInputs, error>
  {
    // TODO: use RAII for fclose/pcap_close
    pair<FILE *> files = {.A = std::fopen(filenames.A.c_str(), "rb"), .B = std::fopen(filenames.B.c_str(), "rb")};
    if (files.A == nullptr && files.B == nullptr) {
      return error::make("failed to open both files: ", filenames.A, ", ", filenames.B);
    }
    if (files.A == nullptr) {
      std::fclose(files.B);
      return error::make("failed to open file A: ", filenames.A);
    }
    if (files.B == nullptr) {
      std::fclose(files.A);
      return error::make("failed to open file B: ", filenames.B);
    }

    char buffer[PCAP_ERRBUF_SIZE + 1] = {};
    pair<pcap_t *> ret = {};
    // https://www.tcpdump.org/manpages/pcap_open_offline.3pcap.html
    ret.A = pcap_fopen_offline(files.A, buffer);
    if (ret.A == nullptr) {
      std::fclose(files.A);
      std::fclose(files.B);
      return error::make("invalid file A, error: ", buffer);
    }

    ret.B = pcap_fopen_offline(files.B, buffer);
    if (ret.B == nullptr) {
      pcap_close(ret.A);
      std::fclose(files.B);
      return error::make("invalid file B, error: ", buffer);
    }

    return PcapInputs(ret);
  }

  // noncopyable, but moveable
  PcapInputs(PcapInputs const &) = delete;
  PcapInputs(PcapInputs &&other) : A_(other.A_), B_(other.B_)
  {
    other.A_ = nullptr;
    other.B_ = nullptr;
  }

  ~PcapInputs()
  {
    // https://www.tcpdump.org/manpages/pcap_close.3pcap.html
    if (A_ != nullptr) {
      pcap_close(A_);
    }
    if (B_ != nullptr) {
      pcap_close(B_);
    }
  }

private:
  explicit PcapInputs(pair<pcap_t *> src) : A_(src.A), B_(src.B) {}

  static auto next_(pcap_t *input, auto &&callback) -> bool
  {
    pcap_pkthdr *pkt_header = nullptr;
    unsigned char const *data = nullptr;
    // https://www.tcpdump.org/manpages/pcap_next_ex.3pcap.html
    if (auto ret = pcap_next_ex(input, &pkt_header, &data); ret == 1) {
      callback(pkt_header->caplen, pkt_header->len, data);
      return true;
    }
    return false;
  }

  auto next_a(std::move_only_function<callback_t> callback) -> bool override { return next_(A_, std::move(callback)); }
  auto next_b(std::move_only_function<callback_t> callback) -> bool override { return next_(B_, std::move(callback)); }

  pcap_t *A_;
  pcap_t *B_;
};

#endif // LIB_PCAP_INPUTS
