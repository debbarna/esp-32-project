# ESP32-Based Washing Machine Control System

Embedded firmware (C/C++, Arduino framework) that drives a multi-stage
washing machine cycle — **Fill → Wash → Rinse → Spin** — and exposes
Wi-Fi based remote monitoring and control via a simple web interface and
JSON status endpoint.

## Features
- **State-machine cycle control** covering fill, wash, drain, rinse,
  second drain, and spin stages, with a water-level sensor gating the
  fill/drain transitions and timers gating the agitate/spin durations.
- **Relay-driven actuation** for inlet valve, drain pump, wash motor,
  spin motor, and door lock solenoid.
- **Wi-Fi connectivity** using the ESP32's onboard radio — connects to a
  home network and hosts a lightweight `WebServer`.
- **Remote monitoring & control**: a mobile-friendly web page (auto
  refreshing) plus a `/status` JSON endpoint and `/start` / `/stop`
  control endpoints, so the cycle can be started, stopped, and observed
  from a phone browser on the same network.

## Hardware
| Component | Notes |
|---|---|
| ESP32 DevKit | Main controller, Wi-Fi enabled |
| 4-channel relay module | Inlet valve, drain pump, wash motor, spin motor |
| Water level sensor | Analog (pressure/float-based), read via ADC |
| Door lock relay/solenoid | Safety interlock during cycle |

### Wiring
| Signal | Pin |
|---|---|
| Inlet valve relay | GPIO 16 |
| Drain pump relay | GPIO 17 |
| Wash motor relay | GPIO 18 |
| Spin motor relay | GPIO 19 |
| Door lock relay | GPIO 21 |
| Water level sensor | GPIO 34 (ADC) |

## API
| Endpoint | Method | Description |
|---|---|---|
| `/` | GET | Web dashboard showing current cycle state |
| `/status` | GET | JSON: `{state, elapsed_ms, water_level}` |
| `/start` | GET | Begins the wash cycle from IDLE |
| `/stop` | GET | Aborts the cycle and unlocks the door |

## Setup
1. Set `WIFI_SSID` and `WIFI_PASSWORD` in `src/washing_machine.ino`.
2. Calibrate `TARGET_WATER_LEVEL` against your sensor's ADC reading at
   the desired fill level.
3. Adjust `WASH_DURATION_MS`, `RINSE_DURATION_MS`, and `SPIN_DURATION_MS`
   for your cycle timing.
4. Flash to an ESP32 via the Arduino IDE / PlatformIO.

## Repo structure
```
src/
  washing_machine.ino   # Main firmware
```

## License
MIT
