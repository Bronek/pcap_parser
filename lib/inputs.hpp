#ifndef LIB_INPUTS
#define LIB_INPUTS

#include "error.hpp"
#include "pair.hpp"

#include <array>
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
  bool next(pair_select which, auto &&fn)
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

private:
  virtual bool next_a(std::move_only_function<callback_t>) = 0;
  virtual bool next_b(std::move_only_function<callback_t>) = 0;
};

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

  static bool next_(pcap_t *input, auto &&callback)
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

  bool next_a(std::move_only_function<callback_t> callback) override { return next_(A_, std::move(callback)); }
  bool next_b(std::move_only_function<callback_t> callback) override { return next_(B_, std::move(callback)); }

  pcap_t *A_;
  pcap_t *B_;
};

#endif // LIB_ERROR
