#pragma once

#include <string>

#include "app_types.h"

NetworkStats calculate_network_stats(
    const std::vector<Observation>& observations);
int network_score(const NetworkStats& stats, const DnsResult& last_dns);
int radio_score(const WifiInfo& wifi);
std::string score_label(int score);
std::string radio_label(const WifiInfo& wifi);
