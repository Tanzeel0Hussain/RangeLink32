# RangeLink32

**RangeLink32 — Smart ESP32 Wi-Fi Extender & Managed Gateway**

[![Firmware Build](https://github.com/Tanzeel0Hussain/RangeLink32/actions/workflows/firmware.yml/badge.svg)](https://github.com/Tanzeel0Hussain/RangeLink32/actions/workflows/firmware.yml)
[![Live Installer](https://img.shields.io/badge/Live-Installer-00b8d9)](https://tanzeel0hussain.github.io/RangeLink32/)
[![Release](https://img.shields.io/badge/Firmware-v1.0.0-42d392)](https://github.com/Tanzeel0Hussain/RangeLink32/releases/tag/v1.0.0)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

RangeLink32 is a managed 2.4 GHz Wi-Fi extender/gateway for a classic **ESP32U with an external antenna**. It connects to an existing Wi-Fi network in station mode, keeps its own local access point available, and forwards Internet traffic to downstream devices through ESP32 NAPT.

## Quick Links

- [Open the Browser Firmware Installer](https://tanzeel0hussain.github.io/RangeLink32/)
- [Download the v1.0.0 Firmware Release](https://github.com/Tanzeel0Hussain/RangeLink32/releases/tag/v1.0.0)
- [Check Firmware Builds](https://github.com/Tanzeel0Hussain/RangeLink32/actions/workflows/firmware.yml)
- [Read the Hardware Notes](docs/HARDWARE.md)

## Features

- STA + AP operation with NAPT Internet forwarding
- Fixed management address at `192.168.50.1`
- Nearby Wi-Fi scan with RSSI, channel and security
- Automatic reconnect with increasing retry backoff
- Multiple saved upstream Wi-Fi profiles and automatic failover
- Per-network cumulative Internet usage
- Current and previously seen client inventory with IP, MAC, RSSI and custom names
- Allow-all and allowlisted-only policies
- Internet-only client blocking while local management remains available
- Per-device download/upload, daily usage and total usage
- Daily data quota and approximate per-device bandwidth cap
- Daily Internet schedules and temporary guest access
- Internet health checks, event logs and watchdog recovery
- Smart Placement Assistant and 2.4 GHz channel-congestion analysis
- Custom downstream DNS
- Locally generated Wi-Fi QR onboarding
- OTA application firmware update
- Safe settings backup/restore without exporting passwords
- Light/dark responsive admin dashboard
- Restart and factory reset controls

## Security

Saved upstream Wi-Fi passwords, the RangeLink32 hotspot password and the admin password are stored in encrypted application form using AES-GCM with a device-derived key. Early plaintext development credentials are migrated when possible.

Saved upstream passwords are excluded from the normal profile API and safe backup. Revealing a saved upstream password requires the administrator to enter the admin password again.

This application-level protection prevents normal plaintext NVS storage, but it is **not a replacement for ESP32 Secure Boot and hardware flash encryption** when resistance to physical extraction is required.

## Initial Access

| Setting | Initial value |
| --- | --- |
| Wi-Fi name | `RangeLink32-Setup` |
| Wi-Fi password | `rangelink32` |
| Management address | `192.168.50.1` |
| Admin username | `admin` |
| Admin password | `changeme32` |

Change the default hotspot and admin credentials after first login.

## Browser Installation

1. Connect the ESP32U using a data-capable USB cable.
2. Open the [RangeLink32 Web Installer](https://tanzeel0hussain.github.io/RangeLink32/) in a Chromium-based browser with Web Serial support.
3. Select **Install RangeLink32**, choose the ESP32 serial port and flash.
4. Join `RangeLink32-Setup`.
5. Open `192.168.50.1` and sign in.
6. Scan for an upstream Wi-Fi network, enter its password and connect.

## Firmware Files

The [v1.0.0 Firmware Release](https://github.com/Tanzeel0Hussain/RangeLink32/releases/tag/v1.0.0) provides:

- **rangelink32-full.bin** — complete first-install image for flashing from address `0x0`.
- **rangelink32-ota.bin** — application image for the dashboard OTA updater.

The browser installer writes the bootloader, partition table, boot-app image and application at their correct ESP32 offsets automatically.

## Build from Source

```bash
pio run -e esp32dev
```

Upload:

```bash
pio run -e esp32dev -t upload
```

Serial monitor:

```bash
pio device monitor -b 115200
```

## Architecture

```text
Internet
   |
Main 2.4 GHz Wi-Fi
   |
   | STA
+-----------------------------+
| ESP32U + External Antenna   |
| RangeLink32                 |
| Wi-Fi manager / failover    |
| NAPT forwarding             |
| Traffic + access policies   |
| Local admin dashboard       |
+-----------------------------+
   | AP
   +----------+----------+
              |          |
            Phone      Laptop
```

## Important Limitations

RangeLink32 uses a **single 2.4 GHz Wi-Fi radio** for upstream and downstream traffic, so it cannot match a commercial dual-radio/dual-band repeater. The ESP32U must receive a usable upstream signal.

Bandwidth limiting is implemented as an embedded packet budget and should be treated as an approximate cap rather than carrier/router-grade QoS. Usage counters are for management visibility, not billing.

## Validation

GitHub Actions compiles the current firmware with Arduino-ESP32 3.3.11 / ESP-IDF 5.5.5. Real ESP32U testing is still required for external-antenna range, sustained NAPT throughput, accounting accuracy and long-duration stability.

## License

RangeLink32 is released under the [MIT License](LICENSE).
