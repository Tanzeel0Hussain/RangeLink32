# RangeLink32

**RangeLink32 — Smart ESP32 Wi-Fi Extender & Managed Gateway**

RangeLink32 is an ESP32U-focused project designed to connect to an existing 2.4 GHz Wi-Fi network, keep a permanent local management access point available, and forward Internet access to approved downstream devices through a router-style admin dashboard.

> **P4 status:** development started. Initial firmware structure, Wi-Fi management, admin dashboard, reconnect logic and routing/access-control interfaces are being built first. Hardware validation will follow on a real ESP32U.

## Target Features

- Automatic upstream Wi-Fi reconnect with retry backoff
- Multiple saved Wi-Fi profiles with priority and failover
- Permanent local management AP
- Fixed management IP: `192.168.50.1`
- Password-protected router-style admin dashboard
- Nearby Wi-Fi scan with RSSI, channel and security
- Configurable RangeLink32 SSID and password
- Saved-network history
- Per-network data-usage accounting
- Current and previous client inventory
- Client hostname, IP and MAC visibility
- Per-device upload/download/total traffic
- MAC allowlist and blocklist
- Per-device Internet enable/disable
- Bandwidth limits and data quotas
- Guest access and schedules
- Persistent event logs
- Encrypted Wi-Fi credential storage
- Re-authentication before revealing saved credentials
- OTA firmware update
- Settings backup/restore
- Factory reset
- QR-code onboarding
- Upstream health checks
- Watchdog/auto recovery
- Channel analysis
- Smart Placement Assistant
- Custom DNS controls
- Responsive admin dashboard

## Initial Development Access

- AP SSID: `RangeLink32-Setup`
- AP password: `rangelink32`
- Admin IP: `192.168.50.1`
- Admin username: `admin`
- Temporary development password: `changeme32`

Change all default credentials before regular use.

## Architecture

```text
Main 2.4 GHz Wi-Fi
        |
   ESP32U + External Antenna
        |
   STA + AP + NAT/NAPT
        |
   RangeLink32 Wi-Fi
      /          \
   Phone        Laptop
```

## Build

```bash
pio run -e esp32dev
```

Upload:

```bash
pio run -e esp32dev -t upload
```

## Development Status

The project is intentionally modular. Wi-Fi management, routing, storage, access control, accounting and the web dashboard are kept separate so advanced features can be added without turning the firmware into one large file.

NAT/NAPT forwarding and accurate per-client accounting depend on lower-level ESP32/lwIP integration and will only be marked complete after compilation and real ESP32U hardware testing.
