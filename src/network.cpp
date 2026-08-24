#include "network.h"

#include <netdb.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <regex>

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
  pclose(pipe);
  return output;
}

}  // namespace

std::string platform_name() {
#if defined(__APPLE__)
  return "macOS";
#elif defined(__linux__)
  return "Linux";
#else
  return "Unknown";
#endif
}

PingResult ping_target(const std::string& target) {
#if defined(__APPLE__)
  const std::string command = "/sbin/ping -n -c 1 -W 1000 " + target + " 2>&1";
#elif defined(__linux__)
  const std::string command = "/bin/ping -n -c 1 -W 1 " + target + " 2>&1";
#else
  const std::string command = "ping -n -c 1 " + target + " 2>&1";
#endif
  const std::string output = run_command(command);
  std::smatch match;
  const std::regex time_pattern("time[=<]([0-9.]+) ?ms");

  PingResult result{};
  if (std::regex_search(output, match, time_pattern)) {
    result.reachable = true;
    result.latency_ms = std::stod(match[1].str());
  }
  return result;
}

DnsResult check_dns() {
  const auto start = std::chrono::steady_clock::now();
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  addrinfo* results = nullptr;
  const int status = getaddrinfo("example.com", nullptr, &hints, &results);
  if (results != nullptr) freeaddrinfo(results);

  DnsResult result{};
  result.successful = status == 0;
  result.duration_ms = std::chrono::duration<double, std::milli>(
                           std::chrono::steady_clock::now() - start)
                           .count();
  return result;
}
