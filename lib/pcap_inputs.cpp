#include "pcap_inputs.hpp"

[[nodiscard]] auto PcapInputs::make(pair<std::string> filenames) -> std::expected<PcapInputs, error>
{
  struct file_closer final {
    void operator()(FILE *f) const noexcept { std::fclose(f); }
  };
  using file_handle = std::unique_ptr<FILE, file_closer>;

  pair<file_handle> files = {.A{std::fopen(filenames.A.c_str(), "rb")}, .B{std::fopen(filenames.B.c_str(), "rb")}};
  if (files.A == nullptr && files.B == nullptr) {
    return error::make("failed to open both files: ", filenames.A, ", ", filenames.B);
  }
  if (files.A == nullptr) {
    return error::make("failed to open file A: ", filenames.A);
  }
  if (files.B == nullptr) {
    return error::make("failed to open file B: ", filenames.B);
  }

  char buffer[PCAP_ERRBUF_SIZE + 1] = {};
  // NOTE: ::pcap_fopen_offline does not close FILE* on error execution paths. This means we need to close it
  // ourselves if this function fails, meaning we cannot use .release() here.
  // https://github.com/the-tcpdump-group/libpcap/blob/master/savefile.c#L467
  pair<pcap_handle> ret = {.A{::pcap_fopen_offline(files.A.get(), buffer)}, .B{nullptr}};
  // https://www.tcpdump.org/manpages/pcap_open_offline.3pcap.html
  if (ret.A == nullptr) {
    return error::make("invalid file A, error: ", buffer);
  }
  [[maybe_unused]] auto *_ = files.A.release(); // now owned by ret.A

  ret.B.reset(::pcap_fopen_offline(files.B.get(), buffer));
  if (ret.B == nullptr) {
    return error::make("invalid file B, error: ", buffer);
  }
  _ = files.B.release(); // now owned by ret.B

  return PcapInputs(std::move(ret));
}
