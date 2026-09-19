# RangeLink32 Security

RangeLink32 is intended for ESP32 hardware and Wi-Fi networks you own or are authorized to administer.

## Credential storage

The maintained firmware stores the hotspot password, administrator password and saved upstream Wi-Fi passwords in AES-GCM protected application form. On first boot it generates a random 256-bit per-device master secret in a separate NVS namespace, mixes that secret with the chip identity to derive the credential-encryption key, and uses a fresh random AES-GCM nonce for every stored value. Legacy plaintext and earlier `enc1:` values are migrated to the newer `enc2:` format when possible.

Saved Wi-Fi passwords are excluded from the normal profile API and safe configuration backup. Password reveal requires administrator authentication plus re-entry of the administrator password.

This is application-level at-rest protection. The random master secret prevents the encryption key from being recreated from the public MAC/chip ID alone, but a complete physical flash/NVS extraction may still expose enough material to recover application secrets. ESP32 Secure Boot and hardware flash encryption are still required when resistance to physical-device extraction is needed.

## Local administration

The management interface is intended for the RangeLink32 local network at `192.168.50.1`. Change the default hotspot and administrator credentials before regular use.

Firmware uploaded through the OTA page should come from the project's own release/build pipeline and must match the target ESP32 hardware.

## Reporting

Do not publish real Wi-Fi passwords, private network inventories, tokens, backup contents or personal device identifiers in public issues.
