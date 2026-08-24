#pragma once

#include "app_types.h"

void start_dashboard();
void stop_dashboard();
void draw_dashboard(const Options& options, const WifiInfo& wifi,
                    const NetworkStats& stats, int score, const DnsResult& dns);
