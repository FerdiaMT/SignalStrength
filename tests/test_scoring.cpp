#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "app_types.h"
#include "scoring.h"

namespace {

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        failures++;
    }
}

Observation sample(bool reachable, double latency_ms) {
    Observation observation{};
    observation.ping.reachable = reachable;
    observation.ping.latency_ms = latency_ms;
    return observation;
}

}  // namespace

int main() {
    DnsResult dns_ok{true, 5.0};

    std::vector<Observation> stable_samples{
        sample(true, 10.0),
        sample(true, 11.0),
        sample(true, 10.0),
        sample(true, 11.0),
    };
    const NetworkStats stable_stats = calculate_network_stats(stable_samples);
    const int stable_score = network_score(stable_stats, dns_ok);
    expect(stable_stats.loss_percent == 0.0, "stable connection should have no loss");
    expect(stable_score >= 85, "stable connection should score as excellent");

    std::vector<Observation> loss_samples{
        sample(true, 10.0),
        sample(false, 0.0),
        sample(true, 10.0),
        sample(false, 0.0),
    };
    const NetworkStats loss_stats = calculate_network_stats(loss_samples);
    const int loss_score = network_score(loss_stats, dns_ok);
    expect(std::abs(loss_stats.loss_percent - 50.0) < 0.01,
           "two failed probes out of four should be 50 percent loss");
    expect(loss_score < stable_score, "packet loss should reduce the score");

    std::vector<Observation> jitter_samples{
        sample(true, 10.0),
        sample(true, 130.0),
        sample(true, 10.0),
        sample(true, 130.0),
    };
    const NetworkStats jitter_stats = calculate_network_stats(jitter_samples);
    const int jitter_score = network_score(jitter_stats, dns_ok);
    expect(jitter_stats.jitter_ms > 100.0, "large latency swings should produce high jitter");
    expect(jitter_score < stable_score, "high jitter should reduce the score");

    WifiInfo unavailable_radio{};
    unavailable_radio.rssi_dbm = NO_INT_VALUE;
    expect(radio_score(unavailable_radio) == 0,
           "missing RSSI must not turn into a perfect signal score");
    expect(radio_label(unavailable_radio) == "UNAVAILABLE",
           "missing RSSI should be labelled unavailable");

    WifiInfo invalid_radio{};
    invalid_radio.rssi_dbm = -1;
    expect(radio_score(invalid_radio) == 0,
           "invalid RSSI must not turn into a perfect signal score");

    if (failures == 0) {
        std::cout << "All scoring tests passed.\n";
        return 0;
    }
    return 1;
}
