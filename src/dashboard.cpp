#include "dashboard.h"

#include <unistd.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "network.h"
#include "scoring.h"

namespace {

std::string make_bar(int score, int width) {
  const int filled = score * width / 100;
  return std::string(filled, '#') + std::string(width - filled, '.');
}

std::string format_int(int value, const std::string& suffix) {
  if (value == NO_INT_VALUE) return "—";
  return std::to_string(value) + suffix;
}

std::string format_double(double value, const std::string& suffix) {
  if (value < 0.0) return "—";
  std::ostringstream output;
  output << std::fixed << std::setprecision(1) << value << suffix;
  return output.str();
}

void draw_box_top(const std::string& title) {
  const int dash_count = std::max(0, 69 - static_cast<int>(title.size()));
  std::cout << "  \033[38;5;245m+-- " << title << " "
            << std::string(dash_count, '-') << "+\033[0m\n";
}

void draw_box_row(std::string text) {
  const int box_width = 72;
  if (static_cast<int>(text.size()) > box_width) text.resize(box_width);
  const int spaces = box_width - static_cast<int>(text.size());
  std::cout << "  \033[38;5;245m|\033[0m " << text << std::string(spaces, ' ')
            << "\033[38;5;245m|\033[0m\n";
}

void draw_box_bottom() {
  std::cout << "  \033[38;5;245m+" << std::string(73, '-') << "+\033[0m\n";
}

}  // namespace

void start_dashboard() {
  if (isatty(STDOUT_FILENO)) {
    std::cout << "\033[?1049h\033[2J\033[?25l" << std::flush;
  }
}

void stop_dashboard() {
  if (isatty(STDOUT_FILENO)) {
    std::cout << "\033[0m\033[?25h\033[?1049l" << std::flush;
  }
}

void draw_dashboard(const Options& options, const WifiInfo& wifi,
                    const NetworkStats& stats, int score,
                    const DnsResult& dns) {
  const std::string cyan = "\033[38;5;81m";
  const std::string dim = "\033[38;5;245m";
  const std::string white = "\033[97m";
  const std::string reset = "\033[0m";
  const int signal = radio_score(wifi);

  std::ostringstream radio_signal;
  if (wifi.rssi_dbm > -20 || wifi.rssi_dbm < -100) {
    radio_signal
        << " Signal   [??????????????????????????]  N/A      UNAVAILABLE";
  } else {
    radio_signal << " Signal   " << make_bar(signal, 26) << "  " << std::setw(3)
                 << signal << "/100  " << radio_label(wifi);
  }

  std::ostringstream radio_details;
  radio_details << " RSSI     " << std::setw(12)
                << format_int(wifi.rssi_dbm, " dBm") << "      Noise  "
                << format_int(wifi.noise_dbm, " dBm");
  std::ostringstream radio_rate;
  radio_rate << " Channel  " << std::setw(12) << format_int(wifi.channel, "")
             << "      Tx rate "
             << format_double(wifi.transmit_rate_mbps, " Mbps");
  std::ostringstream network_quality;
  network_quality << " Quality  " << make_bar(score, 26) << "  " << std::setw(3)
                  << score << "/100  " << score_label(score);
  std::ostringstream network_latency;
  network_latency << " Latency  " << std::setw(8) << std::fixed
                  << std::setprecision(1) << stats.average_latency_ms
                  << " ms      Jitter  " << stats.jitter_ms << " ms";
  std::ostringstream network_loss;
  network_loss << " Loss     " << std::setw(8) << stats.loss_percent
               << "%       DNS     " << (dns.successful ? "HEALTHY" : "FAILED");

  std::cout << "\033[2J\033[H" << cyan << "  SIGNALSTRENGTH" << reset << dim
            << "  /  LIVE WI-FI DIAGNOSTICS" << reset << "\n\n"
            << "  " << white << wifi.ssid << reset << dim << "   •   "
            << platform_name() << "   •   " << options.location << "   •   "
            << options.target << reset << "\n\n";
  draw_box_top("RADIO LINK");
  draw_box_row(radio_signal.str());
  draw_box_row(radio_details.str());
  draw_box_row(radio_rate.str());
  draw_box_bottom();
  std::cout << "\n";
  draw_box_top("NETWORK EXPERIENCE");
  draw_box_row(network_quality.str());
  draw_box_row(network_latency.str());
  draw_box_row(network_loss.str());
  draw_box_bottom();
  std::cout << "\n"
            << dim << "  " << stats.samples << " samples  •  rolling window "
            << ROLLING_WINDOW << "  •  Ctrl-C saves and exits" << reset
            << std::flush;
}
