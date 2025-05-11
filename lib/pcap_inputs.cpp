#include "pcap_inputs.hpp"

namespace {

struct file_closer final {
  void operator()(FILE *file) const noexcept { std::fclose(file); }
};
using file_handle = std::unique_ptr<FILE, file_closer>;

} // namespace

[[nodiscard]] auto PcapInputs::make_t::operator()(pair<std::string> filenames) const -> std::expected<PcapInputs, error>
{
  pair<file_handle> files = {.A = file_handle(std::fopen(filenames.A.c_str(), "rb")),
                             .B = file_handle(std::fopen(filenames.B.c_str(), "rb"))};
  if (files.A == nullptr && files.B == nullptr) {
    return error::make(error::open_pcap, "failed to open both files: ", filenames.A, ", ", filenames.B);
  }
  if (files.A == nullptr) {
    return error::make(error::open_pcap, "failed to open file A: ", filenames.A);
  }
  if (files.B == nullptr) {
    return error::make(error::open_pcap, "failed to open file B: ", filenames.B);
  }

  char buffer[PCAP_ERRBUF_SIZE + 1] = {};
  // NOTE: ::pcap_fopen_offline does not close FILE* on error execution paths.
  // This means we need to close it ourselves if this function fails, meaning we cannot use .release() here.
  // https://github.com/the-tcpdump-group/libpcap/blob/master/savefile.c#L467
  // https://www.tcpdump.org/manpages/pcap_open_offline.3pcap.html
  pair<pcap_handle> ret = {.A = pcap_handle(::pcap_fopen_offline(files.A.get(), buffer)), //
                           .B{nullptr}};
  if (ret.A == nullptr) {
    return error::make(error::open_pcap, "invalid file A, error: ", buffer);
  }
  [[maybe_unused]] auto *_ = files.A.release(); // now owned by ret.A

  ret.B.reset(::pcap_fopen_offline(files.B.get(), buffer));
  if (ret.B == nullptr) {
    return error::make(error::open_pcap, "invalid file B, error: ", buffer);
  }
  _ = files.B.release(); // now owned by ret.B

  return PcapInputs(std::move(ret));
}
