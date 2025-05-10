#include "pcap_inputs.hpp"

[[nodiscard]] auto PcapInputs::make(pair<std::string> filenames) -> std::expected<PcapInputs, error>
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
