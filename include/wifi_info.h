#pragma once

#include <string>

struct WifiInfo {
  std::string ssid{"not-connected"};
  int rssi_dbm{-999};
  int noise_dbm{-999};
  double transmit_rate_mbps{-1.0};
  int channel{-999};
};

WifiInfo read_wifi_info();
