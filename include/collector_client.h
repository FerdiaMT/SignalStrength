#pragma once

#include <string>

bool send_observation_to_collector(const std::string& collector_url,
                                   const std::string& location,
                                   const std::string& target,
                                   int quality_score,
                                   double latency_ms,
                                   double loss_percent,
                                   double jitter_ms,
                                   bool dns_ok);
