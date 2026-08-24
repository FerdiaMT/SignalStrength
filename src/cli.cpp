#include "cli.h"

#include <cstdlib>
#include <iostream>
#include <regex>

namespace {

bool is_safe_target(const std::string& target) {
  static const std::regex allowed("^[A-Za-z0-9.-]+$");
  return !target.empty() && std::regex_match(target, allowed);
}

}  // namespace

void print_usage() {
  std::cout << "Usage:\n"
            << "  signalstrength --location <name>\n"
            << "  signalstrength live --location <name> [--interval-ms "
               "100-60000] [--samples count]\n"
            << "  signalstrength report\n";
}

bool parse_options(int argc, char* argv[], Options* options) {
  options->live_mode = false;
  options->report_mode = false;
  options->target = "1.1.1.1";
  options->interval_ms = DEFAULT_INTERVAL_MS;
  options->sample_limit = 0;

  int index = 1;
  if (index < argc && std::string(argv[index]) == "live") {
    options->live_mode = true;
    index++;
  } else if (index < argc && std::string(argv[index]) == "report") {
    options->report_mode = true;
    index++;
  }

  for (; index < argc; index++) {
    const std::string argument = argv[index];
    if ((argument == "--location" || argument == "-l") && index + 1 < argc) {
      options->location = argv[++index];
    } else if (argument == "--target" && index + 1 < argc) {
      options->target = argv[++index];
    } else if (argument == "--interval-ms" && index + 1 < argc) {
      options->interval_ms = std::atoi(argv[++index]);
    } else if (argument == "--samples" && index + 1 < argc) {
      options->sample_limit = std::atoi(argv[++index]);
    } else {
      return false;
    }
  }

  if (options->report_mode) return true;
  return !options->location.empty() && is_safe_target(options->target) &&
         options->interval_ms >= 100 && options->interval_ms <= 60000 &&
         options->sample_limit >= 0;
}
