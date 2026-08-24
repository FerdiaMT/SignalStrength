#include <array>
#include <cstdio>
#include <regex>
#include <string>

#include "wifi_info.h"

namespace {

std::string run_command(const std::string& command) {
  std::string output;
  std::array<char, 256> buffer{};
  FILE* pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) return output;
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) !=
         nullptr) {
    output += buffer.data();
  }
  (void)pclose(pipe);
  return output;
}

}  // namespace

WifiInfo read_wifi_info() {
  const std::string devices = run_command("iw dev 2>/dev/null");
  std::smatch match;
  const std::regex interface_pattern("Interface ([A-Za-z0-9._-]+)");
  if (!std::regex_search(devices, match, interface_pattern)) return {};

  const std::string interface_name = match[1].str();
  const std::string link =
      run_command("iw dev " + interface_name + " link 2>/dev/null");
  if (link.find("Not connected") != std::string::npos) return {};

  WifiInfo info;
  const std::regex ssid_pattern("SSID: (.+)");
  const std::regex signal_pattern("signal: (-?[0-9]+) dBm");
  const std::regex bitrate_pattern("tx bitrate: ([0-9.]+) MBit/s");
  const std::regex channel_pattern("channel ([0-9]+)");
  if (std::regex_search(link, match, ssid_pattern)) info.ssid = match[1].str();
  if (std::regex_search(link, match, signal_pattern))
    info.rssi_dbm = std::stoi(match[1].str());
  if (std::regex_search(link, match, bitrate_pattern))
    info.transmit_rate_mbps = std::stod(match[1].str());

  const std::string channel_data =
      run_command("iw dev " + interface_name + " info 2>/dev/null");
  if (std::regex_search(channel_data, match, channel_pattern))
    info.channel = std::stoi(match[1].str());
  return info;
}
