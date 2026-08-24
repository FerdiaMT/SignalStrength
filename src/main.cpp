#include <netdb.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

constexpr std::size_t kRollingWindow = 20;
volatile std::sig_atomic_t g_stop_requested = 0;

struct PingResult {
  bool reachable{};
  double latency_ms{};
};

struct DnsResult {
  bool resolved{};
  double duration_ms{};
};

struct Observation {
  std::chrono::system_clock::time_point timestamp;
  PingResult ping;
  std::optional<DnsResult> dns;
};

struct RollingStats {
  int samples{};
  double packet_loss_pct{};
  double average_latency_ms{};
  double jitter_ms{};
};

struct Options {
  bool live{};
  bool report{};
  std::string location;
  std::string target{"1.1.1.1"};
  int interval_ms{500};
  int samples{};  // Zero means run until Ctrl-C.
};

void request_stop(int) { g_stop_requested = 1; }

std::string run_command(const std::string& command) {
  std::string output;
  std::array<char, 256> buffer{};
  FILE* pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) {
    throw std::runtime_error("Unable to start command: " + command);
  }
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) !=
         nullptr) {
    output += buffer.data();
  }
  (void)pclose(pipe);
  return output;
}

bool safe_target(const std::string& target) {
  static const std::regex allowed("^[A-Za-z0-9.-]+$");
  return std::regex_match(target, allowed) && !target.empty();
}

bool safe_interface(const std::string& interface_name) {
  static const std::regex allowed("^[A-Za-z0-9._-]+$");
  return std::regex_match(interface_name, allowed) && !interface_name.empty();
}

std::string trim_newline(std::string value) {
  while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
    value.pop_back();
  }
  return value;
}

// Built on apple and intended for Linux. Should probably make a windows port
// eventually
std::string platform_name() {
#if defined(__APPLE__)
  return "macOS";
#elif defined(__linux__)
  return "Linux";
#else
  return "unknown";
#endif
}

std::string connected_ssid() {
#if defined(__APPLE__)
  const std::string ports =
      run_command("/usr/sbin/networksetup -listallhardwareports 2>/dev/null");
  std::smatch match;
  const std::regex wifi_device(
      "Hardware Port: Wi-Fi[\\s\\S]*?Device: ([A-Za-z0-9]+)");
  if (!std::regex_search(ports, match, wifi_device) ||
      !safe_interface(match[1].str())) {
    return "unknown";
  }
  const std::string output =
      run_command("/usr/sbin/networksetup -getairportnetwork " +
                  match[1].str() + " 2>/dev/null");
  constexpr std::string_view prefix = "Current Wi-Fi Network: ";
  return output.rfind(prefix, 0) == 0
             ? trim_newline(output.substr(prefix.size()))
             : "not-connected";
#elif defined(__linux__)
  const std::string devices = run_command("iw dev 2>/dev/null");
  std::smatch match;
  const std::regex interface_pattern("Interface ([A-Za-z0-9._-]+)");
  if (!std::regex_search(devices, match, interface_pattern) ||
      !safe_interface(match[1].str())) {
    return "unknown";
  }
  const std::string link =
      run_command("iw dev " + match[1].str() + " link 2>/dev/null");
  const std::regex ssid_pattern("SSID: (.+)");
  return std::regex_search(link, match, ssid_pattern)
             ? trim_newline(match[1].str())
             : "not-connected";
#else
  return "unsupported";
#endif
}

PingResult ping_target(const std::string& target) {
#if defined(__APPLE__)
  const std::string command = "/sbin/ping -n -c 1 -W 1000 " + target + " 2>&1";
#elif defined(__linux__)
  const std::string command = "/bin/ping -n -c 1 -W 1 " + target + " 2>&1";
#else
  const std::string command = "ping -n -c 1 " + target + " 2>&1";
#endif
  const std::string output = run_command(command);
  std::smatch match;
  const std::regex timing("time[=<]([0-9.]+) ?ms");
  if (std::regex_search(output, match, timing)) {
    return {true, std::stod(match[1].str())};
  }
  return {false, 0.0};
}

DnsResult measure_dns() {
  const auto start = std::chrono::steady_clock::now();
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  addrinfo* addresses = nullptr;
  const int status = getaddrinfo("example.com", nullptr, &hints, &addresses);
  if (addresses != nullptr) {
    freeaddrinfo(addresses);
  }
  const auto stop = std::chrono::steady_clock::now();
  return {status == 0,
          std::chrono::duration<double, std::milli>(stop - start).count()};
}

RollingStats calculate_stats(const std::deque<Observation>& observations) {
  RollingStats stats;
  stats.samples = static_cast<int>(observations.size());
  if (observations.empty()) return stats;

  int successful = 0;
  double total_latency = 0.0;
  std::vector<double> latencies;
  for (const Observation& observation : observations) {
    if (observation.ping.reachable) {
      ++successful;
      total_latency += observation.ping.latency_ms;
      latencies.push_back(observation.ping.latency_ms);
    }
  }
  stats.packet_loss_pct = 100.0 * (stats.samples - successful) / stats.samples;
  if (successful == 0) return stats;

  stats.average_latency_ms = total_latency / successful;
  if (latencies.size() > 1) {
    double delta_total = 0.0;
    for (std::size_t index = 1; index < latencies.size(); ++index) {
      delta_total += std::abs(latencies[index] - latencies[index - 1]);
    }
    stats.jitter_ms = delta_total / static_cast<double>(latencies.size() - 1);
  }
  return stats;
}

int quality_score(const RollingStats& stats,
                  const std::optional<DnsResult>& latest_dns) {
  if (stats.samples == 0) return 0;
  double score = 100.0;
  score -= std::min(70.0, stats.packet_loss_pct * 2.0);
  score -= std::min(20.0, std::max(0.0, stats.average_latency_ms - 20.0) / 8.0);
  score -= std::min(15.0, stats.jitter_ms / 2.0);
  if (latest_dns.has_value() && !latest_dns->resolved) score -= 20.0;
  return std::clamp(static_cast<int>(std::lround(score)), 0, 100);
}

std::string quality_label(int score) {
  if (score >= 85) return "Excellent";
  if (score >= 70) return "Good";
  if (score >= 50) return "Fair";
  return "Poor";
}

std::string quality_bar(int score) {
  constexpr int width = 30;
  const int filled = score * width / 100;
  return "[" + std::string(filled, '#') + std::string(width - filled, '-') +
         "]";
}

std::string timestamp_iso8601(std::chrono::system_clock::time_point timestamp) {
  const std::time_t raw_time = std::chrono::system_clock::to_time_t(timestamp);
  std::tm local_time{};
#if defined(__APPLE__) || defined(__linux__)
  localtime_r(&raw_time, &local_time);
#endif
  std::ostringstream output;
  output << std::put_time(&local_time, "%Y-%m-%dT%H:%M:%S");
  return output.str();
}

std::string csv_escape(const std::string& value) {
  std::string escaped{"\""};
  for (char character : value) {
    escaped += character == '\"' ? "\"\"" : std::string(1, character);
  }
  return escaped + "\"";
}

void append_live_measurement(const Options& options, const std::string& ssid,
                             const Observation& observation,
                             const RollingStats& stats, int score) {
  std::filesystem::create_directories("data");
  const std::filesystem::path file{"data/live_measurements.csv"};
  const bool needs_header =
      !std::filesystem::exists(file) || std::filesystem::file_size(file) == 0;
  std::ofstream stream(file, std::ios::app);
  if (!stream) throw std::runtime_error("Unable to open live measurement file");
  if (needs_header) {
    stream << "timestamp,platform,location,target,ssid,reachable,latency_ms,"
              "rolling_loss_pct,rolling_avg_latency_ms,rolling_jitter_ms,dns_"
              "ok,dns_ms,quality_score\n";
  }
  stream << timestamp_iso8601(observation.timestamp) << ','
         << csv_escape(platform_name()) << ',' << csv_escape(options.location)
         << ',' << csv_escape(options.target) << ',' << csv_escape(ssid) << ','
         << (observation.ping.reachable ? "true" : "false") << ',' << std::fixed
         << std::setprecision(2) << observation.ping.latency_ms << ','
         << stats.packet_loss_pct << ',' << stats.average_latency_ms << ','
         << stats.jitter_ms << ',';
  if (observation.dns.has_value()) {
    stream << (observation.dns->resolved ? "true" : "false") << ','
           << observation.dns->duration_ms;
  } else {
    stream << ",";
  }
  stream << ',' << score << '\n';
}

void print_live_line(const RollingStats& stats, int score,
                     const std::optional<DnsResult>& latest_dns) {
  std::cout << '\r' << quality_bar(score) << ' ' << std::setw(3) << score
            << "/100 " << std::left << std::setw(9) << quality_label(score)
            << std::right << " | " << std::fixed << std::setprecision(1)
            << stats.average_latency_ms << " ms"
            << " | jitter " << stats.jitter_ms << " ms"
            << " | loss " << stats.packet_loss_pct << "%"
            << " | samples " << stats.samples;
  if (latest_dns.has_value())
    std::cout << " | DNS " << (latest_dns->resolved ? "ok" : "failed");
  std::cout << "     " << std::flush;
}

void run_live(const Options& options) {
  const std::string ssid = connected_ssid();
  std::deque<Observation> observations;
  std::optional<DnsResult> latest_dns;
  std::cout << "SignalScout live mode on " << platform_name()
            << " | location: " << options.location << " | Wi-Fi: " << ssid
            << " | target: " << options.target << "\n"
            << "Sampling every " << options.interval_ms
            << " ms. Press Ctrl-C to finish.\n";

  for (int index = 0;
       !g_stop_requested && (options.samples == 0 || index < options.samples);
       ++index) {
    const auto sample_start = std::chrono::steady_clock::now();
    Observation observation{.timestamp = std::chrono::system_clock::now(),
                            .ping = ping_target(options.target)};
    if (index % 10 == 0) {
      latest_dns = measure_dns();
      observation.dns = latest_dns;
    }
    observations.push_back(observation);
    if (observations.size() > kRollingWindow) observations.pop_front();
    const RollingStats stats = calculate_stats(observations);
    const int score = quality_score(stats, latest_dns);
    append_live_measurement(options, ssid, observation, stats, score);
    print_live_line(stats, score, latest_dns);

    const auto elapsed = std::chrono::steady_clock::now() - sample_start;
    const auto interval = std::chrono::milliseconds(options.interval_ms);
    if (elapsed < interval) std::this_thread::sleep_for(interval - elapsed);
  }
  std::cout << "\nLive session saved to data/live_measurements.csv\n";
}

void run_sample(const Options& options) {
  const std::string ssid = connected_ssid();
  const PingResult ping = ping_target(options.target);
  const DnsResult dns = measure_dns();
  const Observation observation{
      .timestamp = std::chrono::system_clock::now(), .ping = ping, .dns = dns};
  const std::deque<Observation> observations{observation};
  const RollingStats stats = calculate_stats(observations);
  const int score = quality_score(stats, dns);
  append_live_measurement(options, ssid, observation, stats, score);
  std::cout << "SignalScout sample saved\n"
            << "  location: " << options.location << "\n"
            << "  platform: " << platform_name() << "\n"
            << "  Wi-Fi: " << ssid << "\n"
            << "  latency: " << std::fixed << std::setprecision(2)
            << ping.latency_ms << " ms\n"
            << "  DNS lookup: " << dns.duration_ms << " ms ("
            << (dns.resolved ? "ok" : "failed") << ")\n"
            << "  quality: " << score << "/100 (" << quality_label(score)
            << ")\n";
}

std::vector<std::string> parse_csv_line(const std::string& line) {
  std::vector<std::string> fields;
  std::string field;
  bool quoted = false;
  for (std::size_t index = 0; index < line.size(); ++index) {
    const char character = line[index];
    if (character == '"') {
      if (quoted && index + 1 < line.size() && line[index + 1] == '"') {
        field += character;
        ++index;
      } else {
        quoted = !quoted;
      }
    } else if (character == ',' && !quoted) {
      fields.push_back(field);
      field.clear();
    } else {
      field += character;
    }
  }
  fields.push_back(field);
  return fields;
}

struct LocationSummary {
  int samples{};
  int unreachable{};
  double total_score{};
  double total_latency{};
};

void run_report() {
  std::ifstream stream("data/live_measurements.csv");
  if (!stream) {
    std::cout << "No live sessions found yet. Start one with:\n"
              << "  ./bin/signalscout live --location \"desk\"\n";
    return;
  }

  std::string line;
  std::getline(stream, line);  // Header
  std::map<std::string, LocationSummary> summaries;
  while (std::getline(stream, line)) {
    const std::vector<std::string> fields = parse_csv_line(line);
    if (fields.size() != 13) continue;
    try {
      LocationSummary& summary = summaries[fields[2]];
      ++summary.samples;
      summary.unreachable += fields[5] == "true" ? 0 : 1;
      summary.total_latency += std::stod(fields[6]);
      summary.total_score += std::stod(fields[12]);
    } catch (const std::exception&) {
      // Ignore malformed rows rather than making one bad observation hide a
      // useful report.
    }
  }
  if (summaries.empty()) {
    std::cout << "No valid observations found in data/live_measurements.csv.\n";
    return;
  }

  struct RankedLocation {
    std::string name;
    LocationSummary summary;
    double average_score{};
  };
  std::vector<RankedLocation> ranked;
  ranked.reserve(summaries.size());
  for (const auto& [name, summary] : summaries) {
    ranked.push_back({name, summary, summary.total_score / summary.samples});
  }
  std::sort(ranked.begin(), ranked.end(),
            [](const RankedLocation& left, const RankedLocation& right) {
              return left.average_score > right.average_score;
            });

  std::cout << "SignalScout location report\n";
  std::cout << std::left << std::setw(22) << "LOCATION" << std::right
            << std::setw(10) << "SCORE" << std::setw(12) << "LATENCY"
            << std::setw(14) << "UNREACHABLE" << '\n';
  for (const RankedLocation& location : ranked) {
    const double average_latency =
        location.summary.total_latency / location.summary.samples;
    std::cout << std::left << std::setw(22) << location.name << std::right
              << std::fixed << std::setprecision(1) << std::setw(10)
              << location.average_score << std::setw(10) << average_latency
              << " ms" << std::setw(10) << location.summary.unreachable << '/'
              << location.summary.samples << '\n';
  }

  const RankedLocation& weakest = ranked.back();
  std::cout << "\nWeakest location: " << weakest.name << " (" << std::fixed
            << std::setprecision(1) << weakest.average_score
            << "/100 average). ";
  if (weakest.summary.unreachable > 0) {
    std::cout << "It had unreachable probes, indicating coverage or "
                 "connectivity instability.\n";
  } else {
    std::cout << "It remained reachable, but its rolling quality score was the "
                 "lowest.\n";
  }
}

void print_usage() {
  std::cout << "Usage:\n"
            << "  signalscout --location <name> [--target <host-or-ip>]\n"
            << "  signalscout live --location <name> [--target <host-or-ip>] "
               "[--interval-ms <100-60000>] [--samples <count>]\n"
            << "  signalscout report\n";
}

std::optional<Options> parse_options(int argc, char* argv[]) {
  Options options;
  int index = 1;
  if (index < argc && std::string_view(argv[index]) == "live") {
    options.live = true;
    ++index;
  } else if (index < argc && std::string_view(argv[index]) == "report") {
    options.report = true;
    ++index;
  } else if (index < argc && std::string_view(argv[index]) == "sample") {
    ++index;
  }
  for (; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    if ((argument == "--location" || argument == "-l") && index + 1 < argc) {
      options.location = argv[++index];
    } else if (argument == "--target" && index + 1 < argc) {
      options.target = argv[++index];
    } else if (argument == "--interval-ms" && index + 1 < argc) {
      options.interval_ms = std::atoi(argv[++index]);
    } else if (argument == "--samples" && index + 1 < argc) {
      options.samples = std::atoi(argv[++index]);
    } else {
      return std::nullopt;
    }
  }
  if ((!options.report && options.location.empty()) ||
      !safe_target(options.target) || options.interval_ms < 100 ||
      options.interval_ms > 60000 || options.samples < 0) {
    return std::nullopt;
  }
  return options;
}

}  // namespace

int main(int argc, char* argv[]) {
  const std::optional<Options> options = parse_options(argc, argv);
  if (!options.has_value()) {
    print_usage();
    return 2;
  }
  std::signal(SIGINT, request_stop);
  try {
    if (options->report) {
      run_report();
    } else if (options->live) {
      run_live(*options);
    } else {
      run_sample(*options);
    }
  } catch (const std::exception& error) {
    std::cerr << "SignalScout failed: " << error.what() << '\n';
    return 1;
  }
  return 0;
}
