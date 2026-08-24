# SignalStrength

SignalStrength is a terminal WiFi tool. It records:

- **Radio signal** — RSSI, noise, channel, and transmit rate (What a WIFI icon represents)
- **Network experience** — packet loss, latency, jitter, Dns health

The live dashboard makes both visible while you walk through your house

## Run on macOS

```bash
make
./bin/signalstrength --location "desk"
./bin/signalstrength live --location "bedroom corner"
./bin/signalstrength live --location "kitchen" --interval-ms 100 --samples 60
```
