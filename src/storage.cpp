#include "storage.h"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "network.h"

namespace {

std::string timestamp_now() {
  const std::time_t raw_time = std::time(nullptr);
  std::tm local_time{};
  localtime_r(&raw_time, &local_time);
  std::ostringstream output;
  output << std::put_time(&local_time, "%Y-%m-%dT%H:%M:%S");
  return output.str();
}

std::string csv_escape(const std::string& text) {
  std::string escaped = "\"";
  for (char character : text) {
    if (character == '"') escaped += "\"";
    escaped += character;
  }
  return escaped + "\"";
}

}  // namespace

void save_observation(const Options& options, const Observation& observation,
                      const NetworkStats& stats, int score) {
  std::filesystem::create_directories("data");
  const char* filename = "data/live_measurements_v2.csv";
  const bool write_header = !std::filesystem::exists(filename) ||
                            std::filesystem::file_size(filename) == 0;
  std::ofstream file(filename, std::ios::app);
  if (!file) {
    std::cerr << "Could not write " << filename << "\n";
    return;
  }
  if (write_header) {
    file << "timestamp,platform,location,target,ssid,rssi_dbm,noise_dbm,tx_"
            "rate_mbps,channel,"
         << "reachable,latency_ms,rolling_loss_pct,rolling_avg_latency_ms,"
            "rolling_jitter_ms,"
         << "dns_ok,dns_ms,quality_score\n";
  }
  file << timestamp_now() << ',' << csv_escape(platform_name()) << ','
       << csv_escape(options.location) << ',' << csv_escape(options.target)
       << ',' << csv_escape(observation.wifi.ssid) << ','
       << observation.wifi.rssi_dbm << ',' << observation.wifi.noise_dbm << ','
       << observation.wifi.transmit_rate_mbps << ',' << observation.wifi.channel
       << ',' << (observation.ping.reachable ? "true" : "false") << ','
       << observation.ping.latency_ms << ',' << stats.loss_percent << ','
       << stats.average_latency_ms << ',' << stats.jitter_ms << ','
       << (observation.dns.successful ? "true" : "false") << ','
       << observation.dns.duration_ms << ',' << score << '\n';
}
