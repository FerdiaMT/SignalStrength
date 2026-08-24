#include "scoring.h"

#include <algorithm>
#include <cmath>

NetworkStats calculate_network_stats(
    const std::vector<Observation>& observations) {
  NetworkStats stats{};
  stats.samples = static_cast<int>(observations.size());
  if (stats.samples == 0) return stats;

  int successful_pings = 0;
  double latency_total = 0.0;
  double previous_latency = 0.0;
  double latency_change_total = 0.0;
  bool has_previous_latency = false;

  for (const Observation& observation : observations) {
    if (!observation.ping.reachable) continue;
    successful_pings++;
    latency_total += observation.ping.latency_ms;
    if (has_previous_latency) {
      latency_change_total +=
          std::abs(observation.ping.latency_ms - previous_latency);
    }
    previous_latency = observation.ping.latency_ms;
    has_previous_latency = true;
  }

  stats.loss_percent =
      100.0 * (stats.samples - successful_pings) / stats.samples;
  if (successful_pings > 0)
    stats.average_latency_ms = latency_total / successful_pings;
  if (successful_pings > 1)
    stats.jitter_ms = latency_change_total / (successful_pings - 1);
  return stats;
}

int network_score(const NetworkStats& stats, const DnsResult& last_dns) {
  if (stats.samples == 0) return 0;

  double score = 100.0;
  score -= std::min(70.0, stats.loss_percent * 2.0);
  score -= std::min(20.0, std::max(0.0, stats.average_latency_ms - 20.0) / 8.0);
  score -= std::min(15.0, stats.jitter_ms / 2.0);
  if (!last_dns.successful) score -= 20.0;
  return std::clamp(static_cast<int>(score), 0, 100);
}

int radio_score(const WifiInfo& wifi) {
  if (wifi.rssi_dbm > -20 || wifi.rssi_dbm < -100) return 0;
  return std::clamp((wifi.rssi_dbm + 90) * 100 / 45, 0, 100);
}

std::string score_label(int score) {
  if (score >= 85) return "EXCELLENT";
  if (score >= 70) return "GOOD";
  if (score >= 50) return "FAIR";
  return "POOR";
}

std::string radio_label(const WifiInfo& wifi) {
  if (wifi.rssi_dbm > -20 || wifi.rssi_dbm < -100) return "UNAVAILABLE";
  return score_label(radio_score(wifi));
}
