#pragma once

#include "app_types.h"

void save_observation(const Options& options, const Observation& observation,
                      const NetworkStats& stats, int score);
