#pragma once

#include <string>
#include <vector>

#include "wifi_info.h"

const int NO_INT_VALUE = -999;
const int ROLLING_WINDOW = 20;
const int DEFAULT_INTERVAL_MS = 500;

struct PingResult {
  bool reachable;
  double latency_ms;
};

struct DnsResult {
  bool successful;
  double duration_ms;
};

struct Observation {
  PingResult ping;
  DnsResult dns;
  bool dns_was_checked;
  WifiInfo wifi;
};

struct NetworkStats {
  int samples;
  double loss_percent;
  double average_latency_ms;
  double jitter_ms;
};

struct Options {
  bool live_mode;
  bool report_mode;
  std::string location;
  std::string target;
  int interval_ms;
  int sample_limit;
};
