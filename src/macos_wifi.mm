#import <CoreWLAN/CoreWLAN.h>

#include "wifi_info.h"

WifiInfo read_wifi_info() {
    @autoreleasepool {
        CWInterface* interface = [CWWiFiClient sharedWiFiClient].interface;
        if (interface == nil) {
            return {};
        }

        WifiInfo info;
        if (interface.ssid != nil) {
            info.ssid = std::string([interface.ssid UTF8String]);
        } else {
            // Modern macOS may hide SSID without Location Services permission.
            // RSSI and other radio measurements can still be useful.
            info.ssid = "Wi-Fi (SSID hidden)";
        }
        const NSInteger rssi = interface.rssiValue;
        const NSInteger noise = interface.noiseMeasurement;
        // CoreWLAN uses 0 to mean an error or no active network.
        // Zero is not a valid RSSI value, so keep the C++ sentinel instead.
        if (rssi != 0) info.rssi_dbm = static_cast<int>(rssi);
        if (noise != 0) info.noise_dbm = static_cast<int>(noise);
        info.transmit_rate_mbps = interface.transmitRate;
        if (interface.wlanChannel != nil) {
            info.channel = static_cast<int>(interface.wlanChannel.channelNumber);
        }
        return info;
    }
}
