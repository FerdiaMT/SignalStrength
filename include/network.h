#pragma once

#include <string>

#include "app_types.h"

std::string platform_name();
PingResult ping_target(const std::string& target);
DnsResult check_dns();
