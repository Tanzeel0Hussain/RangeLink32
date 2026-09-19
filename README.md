<div align="center">

<img src="assets/rangelink32-hero.webp" alt="RangeLink32 3D project hero" width="100%">

# RangeLink32

### Smart ESP32U Wi-Fi Extender & Managed Gateway

[![Firmware Build](https://github.com/Tanzeel0Hussain/RangeLink32/actions/workflows/firmware.yml/badge.svg)](https://github.com/Tanzeel0Hussain/RangeLink32/actions/workflows/firmware.yml)
[![Live Site](https://img.shields.io/badge/Live-Project_Site-15c8ff)](https://tanzeel0hussain.github.io/RangeLink32/)
[![Firmware](https://img.shields.io/badge/Firmware-v1.0.0-43e5a3)](https://github.com/Tanzeel0Hussain/RangeLink32/releases/tag/v1.0.0)
[![ESP32](https://img.shields.io/badge/Target-ESP32U-0f88ff)](docs/HARDWARE.md)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

**Extend Wi-Fi. Control access. Manage every device from a local router-style dashboard.**

[Live Project Site](https://tanzeel0hussain.github.io/RangeLink32/) · [Firmware Release](https://github.com/Tanzeel0Hussain/RangeLink32/releases/tag/v1.0.0) · [Builds](https://github.com/Tanzeel0Hussain/RangeLink32/actions/workflows/firmware.yml) · [Hardware Notes](docs/HARDWARE.md)

</div>

---

## Overview

RangeLink32 is a managed **2.4 GHz Wi-Fi extender/gateway** for a classic **ESP32U with an external antenna**. It connects to an authorized upstream Wi-Fi network in station mode, keeps its own local management access point available, and forwards Internet traffic to downstream devices through ESP32 NAPT.

The project is designed around a router-like workflow: scan networks, save trusted upstream profiles, reconnect automatically, inspect connected devices, apply MAC-based access rules, set data/speed limits, review logs, update firmware and manage the system from a responsive local dashboard.

## Highlights

| Network | Device control | Usage management | System |
|---|---|---|---|
| STA + AP + NAPT | MAC/IP inventory | Upload/download counters | Local admin dashboard |
| Auto reconnect | Allowlist / block | Daily quotas | OTA firmware update |
| Saved profiles | Guest access | Monthly quotas | Safe backup / restore |
| Automatic failover | Daily schedules | Approx. speed caps | Event logs + watchdog |
| RSSI + channel scan | Custom device names | Reset daily/monthly usage | Smart placement assistant |

## Architecture

```text
                    Internet
                       │
                Main 2.4 GHz Wi-Fi
                       │
                     STA
            ┌──────────────────────┐
            │      RangeLink32     │
            │   ESP32U + antenna   │
            │                      │
            │  Wi-Fi / failover    │
            │  NAPT forwarding     │
            │  Traffic accounting  │
            │  Access policies     │
            │  Local web admin     │
            └──────────┬───────────┘
                       │ AP
             ┌─────────┴─────────┐
             │                   │
           Phone               Laptop
```

## Core Features

- **Wi-Fi extension:** classic ESP32/ESP32U STA + AP operation with NAPT Internet forwarding.
- **Recovery:** increasing reconnect backoff, multiple saved profiles and automatic failover.
- **Client management:** current and previously seen devices with MAC, IP, RSSI and custom names.
- **Access policies:** allow-all, allowlist-only, manual Internet block/unblock and temporary guest access.
- **Data controls:** total, daily and monthly traffic accounting with configurable daily/monthly MB or GB quotas.
- **Speed controls:** approximate per-device Kbps/Mbps bandwidth caps.
- **Schedules:** time-based daily Internet windows per device.
- **Security:** protected local admin, application-level encrypted credential storage, re-authentication before saved-password reveal.
- **Operations:** event logs, Internet health checks, watchdog recovery, custom DNS and factory reset.
- **Updates:** browser first-install flow plus dashboard OTA update.
- **UX:** light/dark responsive admin UI, locally generated Wi-Fi QR onboarding and Smart Placement Assistant.

## Quick Start

1. Connect the ESP32U to a computer using a data-capable USB cable.
2. Open the **[RangeLink32 Browser Installer](https://tanzeel0hussain.github.io/RangeLink32/)** in a Chromium-based desktop browser with Web Serial support.
3. Select **Install RangeLink32** and flash the device.
4. Join **`RangeLink32-Setup`** using the initial password **`rangelink32`**.
5. Open **`192.168.50.1`**.
6. Sign in with **`admin` / `changeme32`**, then change both default credentials.
7. Scan for your authorized upstream Wi-Fi network, save it and connect.

## Browser & Release Firmware

The **[v1.0.0 release](https://github.com/Tanzeel0Hussain/RangeLink32/releases/tag/v1.0.0)** contains:

| File | Purpose |
|---|---|
| `rangelink32-full.bin` | Combined first-install image for flashing from `0x0` |
| `rangelink32-ota.bin` | Application image for the local OTA updater |

The browser installer writes the bootloader, partition table, boot-app image and application at the correct offsets automatically.

## Build from Source

```bash
git clone https://github.com/Tanzeel0Hussain/RangeLink32.git
cd RangeLink32
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

## Project Gallery — 8 Flexible Slots

The live site already uses an **auto-fit responsive gallery**. These slots are intentionally reserved for your real project images. If the final gallery has fewer or more images, the website grid can be adjusted without changing the overall design.

| Slot | Suggested image |
|---:|---|
| 01 | Dashboard overview |
| 02 | Mobile responsive view |
| 03 | Wi-Fi scanner |
| 04 | Connected client manager |
| 05 | Speed + daily/monthly limits |
| 06 | MAC allow/block controls |
| 07 | Saved Wi-Fi / failover page |
| 08 | ESP32U + external antenna hardware |

> Recommended folder for future images: `docs/screenshots/`. Use WebP or optimized PNG files and descriptive names.

## Initial Access

| Setting | Default |
|---|---|
| Wi-Fi SSID | `RangeLink32-Setup` |
| Wi-Fi password | `rangelink32` |
| Management IP | `192.168.50.1` |
| Admin username | `admin` |
| Admin password | `changeme32` |

Change the hotspot and admin credentials immediately after first setup.

## Security Model

Saved upstream Wi-Fi passwords, the RangeLink32 hotspot password and the admin password are stored in protected application form using AES-GCM with a device-derived key. Saved upstream secrets are not included in the normal profile API or safe settings backup, and revealing one requires administrator authentication plus password re-entry.

For stronger resistance to physical extraction, ESP32 Secure Boot and hardware flash encryption are separate hardware/firmware-hardening layers.

## Important Limitations

RangeLink32 uses a **single 2.4 GHz radio** for both upstream and downstream traffic. It therefore cannot match a commercial dual-radio/dual-band repeater. The ESP32U also needs a usable signal from the upstream router.

Bandwidth caps are implemented as embedded traffic shaping and should be treated as approximate rather than carrier/router-grade QoS. Usage counters are intended for local management visibility, not billing.

## Validation Status

- GitHub Actions firmware build: **passing**
- Browser firmware generation: **passing**
- GitHub Pages deployment: **passing**
- Physical ESP32U range/throughput/stability validation: **still required**

## Repository Structure

```text
RangeLink32/
├── assets/                 # Product site visuals/styles/scripts
├── docs/
│   ├── firmware/           # Generated browser-installable images
│   └── HARDWARE.md
├── firmware/
│   ├── include/
│   └── src/
├── .github/workflows/
├── index.html              # Product-style GitHub Pages site
├── manifest.json           # ESP Web Tools manifest
├── platformio.ini
├── SECURITY.md
└── LICENSE
```

## License

RangeLink32 is released under the [MIT License](LICENSE).

---

<div align="center">

**RangeLink32 — More range. More control.**

</div>
