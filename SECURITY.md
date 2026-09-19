# RangeLink32 Security

RangeLink32 is intended for ESP32 hardware and Wi-Fi networks you own or are authorized to administer.

## Credential storage

The maintained firmware stores the hotspot password, administrator password and saved upstream Wi-Fi passwords in AES-GCM protected application form using a device-derived key. Existing plaintext values from early development builds are migrated when possible.

Saved Wi-Fi passwords are excluded from the normal profile API and safe configuration backup. Password reveal requires administrator authentication plus re-entry of the administrator password.

This is application-level at-rest protection. It does not replace ESP32 Secure Boot or hardware flash encryption when physical-device extraction resistance is required.

## Local administration

The management interface is intended for the RangeLink32 local network at `192.168.50.1`. Change the default hotspot and administrator credentials before regular use.

Firmware uploaded through the OTA page should come from the project's own release/build pipeline and must match the target ESP32 hardware.

## Reporting

Do not publish real Wi-Fi passwords, private network inventories, tokens, backup contents or personal device identifiers in public issues.
