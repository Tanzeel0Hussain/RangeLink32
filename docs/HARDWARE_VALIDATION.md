# RangeLink32 ESP32U Hardware Validation

This checklist is the final acceptance gate for behavior that CI cannot prove on physical radio hardware.

## Test setup

- Classic ESP32 / ESP32U-compatible board matching the `esp32dev` target.
- Correct external 2.4 GHz antenna connected according to the board/vendor RF guidance.
- Stable USB power supply and data cable.
- One authorized 2.4 GHz upstream router/hotspot.
- At least two client devices (for example a phone and laptop).
- A computer with PlatformIO / serial monitor access for recovery testing.

Record the exact board/module marking, flash size, antenna model, power source, upstream router model, and firmware release used.

## Acceptance checklist

| Test | Procedure | Pass condition |
|---|---|---|
| First install | Flash the release `rangelink32-full.bin` or use the browser installer | Device boots without reset loop and `RangeLink32-Setup` appears |
| Mandatory credential setup | Sign in with setup credentials and set separate new Wi-Fi/admin passwords | Normal dashboard stays locked until both factory passwords are replaced |
| AP persistence | Power-cycle after setup | New hotspot/admin credentials remain valid |
| Upstream secured Wi-Fi | Save an authorized WPA2/WPA3-compatible 2.4 GHz profile | ESP32 connects and dashboard shows upstream SSID/RSSI |
| Open upstream Wi-Fi | Use an authorized open test hotspot | Profile connects without a password and does not retain a stale secret |
| NAPT Internet | Join RangeLink32 from a client and browse/ping Internet services | Client reaches Internet through the ESP32 while local admin remains reachable |
| DNS inheritance | Leave custom DNS blank | Client DNS works using upstream-provided DNS; fallback works if upstream DNS is unavailable |
| Internet health | Temporarily block one health endpoint | Dashboard remains Online if another configured health target succeeds |
| Failover | Save two authorized upstream profiles, then disable the active upstream | RangeLink32 reconnects/fails over according to priority and availability |
| Allowlist | Enable allowlist-only and test approved/unapproved clients | Unapproved client keeps local management/DHCP access but cannot use routed Internet |
| Block/unblock | Block an active client, then unblock it | Internet forwarding stops and resumes for that device |
| Daily quota | Set a small test quota | Internet stops at the configured approximate packet boundary and resumes after reset/new day |
| Monthly quota | Set a small monthly quota | Internet stops at limit and Reset Month clears only monthly accounting |
| Speed cap | Apply a low Kbps/Mbps cap | Sustained transfer is visibly limited; treat result as approximate shaping, not carrier QoS |
| Schedule | Configure a short schedule window | Scheduled device is blocked outside the window; before NTP sync it fails closed |
| Guest access | Grant a short guest duration | Access expires automatically after synchronized time passes |
| Client history | Connect/disconnect multiple devices and use Forget Device | Offline unmanaged records can be removed/reclaimed without affecting managed devices |
| Safe backup | Export, change settings, then restore | Non-secret settings/client policies restore; saved Wi-Fi/admin passwords are not exported |
| Invalid backup | Corrupt a backup line and restore | Restore is rejected and pre-existing settings remain unchanged |
| OTA integrity | Upload official OTA binary with matching release SHA-256 | Update succeeds and reboots into the new firmware |
| OTA rejection | Use a wrong SHA-256 or modified binary | Update is rejected and current firmware remains bootable |
| Credential recovery | Test on a sacrificial device/NVS image by corrupting critical stored credentials | Public defaults are not used; random recovery credentials are printed to serial and setup is required |
| Factory reset | Trigger factory reset | Settings and credential master key are cleared; next boot returns to mandatory setup with a fresh key |
| Watchdog/recovery | Run normal traffic while exercising dashboard for 1 hour | No watchdog reset or lock-up |
| Soak test | Run normal traffic for 12–24 hours | No reset loop, progressive heap failure, or loss of routing/admin access |
| Range survey | Measure RSSI/throughput at multiple distances/locations | Results are recorded as measured hardware data, not estimated marketing claims |

## Recommended evidence

For a final hardware-validated release, keep:

1. Serial boot log.
2. Screenshot of dashboard with upstream/NAPT/clients.
3. Failover test timestamps.
4. Before/after quota and speed-test measurements.
5. OTA success and wrong-hash rejection evidence.
6. 12–24 hour soak-test uptime and free-heap snapshots.
7. Range table with distance, RSSI and throughput.
8. Board/module and antenna photos.

## Release gate

Do not label RF range, sustained throughput, quota accuracy, speed shaping, or long-duration stability as hardware-verified until the corresponding test above has been performed on the actual target board. CI compile success is a software validation, not a radio/hardware validation.
