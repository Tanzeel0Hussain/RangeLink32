#include <Preferences.h>
#include <esp_system.h>
#include <cerrno>
#include <climits>
#include "storage.h"
#include "config.h"
#include "crypto_store.h"
#include "text_utils.h"

namespace {
Preferences prefs;
constexpr uint8_t MAX_EVENT_LOGS = 20;
uint32_t bootSequence = 0;

bool credentialRecoveryMode = false;
String recoveryApSsid;
String recoveryApPassword;
String recoveryAdminPassword;

String randomRecoveryPassword(size_t length) {
  static const char alphabet[] =
    "ABCDEFGHJKLMNPQRSTUVWXYZ"
    "abcdefghijkmnopqrstuvwxyz"
    "23456789";

  String password;
  password.reserve(length);

  for (size_t i = 0; i < length; ++i) {
    password += alphabet[
      esp_random() % (sizeof(alphabet) - 1)
    ];
  }

  return password;
}

void ensureRecoveryCredentials() {
  if (
    recoveryApPassword.length() >= 12 &&
    recoveryAdminPassword.length() >= 12
  ) {
    return;
  }

  const uint64_t chipId = ESP.getEfuseMac();

  char suffix[9];
  snprintf(
    suffix,
    sizeof(suffix),
    "%08lX",
    static_cast<unsigned long>(
      chipId & 0xFFFFFFFFULL
    )
  );

  recoveryApSsid =
    "RangeLink32-Recovery-" +
    String(suffix).substring(4);

  recoveryApPassword =
    randomRecoveryPassword(16);

  recoveryAdminPassword =
    randomRecoveryPassword(18);
}

void enterCredentialRecoveryMode(
  const String& reason
) {
  if (credentialRecoveryMode) return;

  credentialRecoveryMode = true;
  ensureRecoveryCredentials();

  Serial.println();
  Serial.println(
    "=== RangeLink32 credential recovery ==="
  );
  Serial.println(reason);
  Serial.print("Recovery Wi-Fi: ");
  Serial.println(recoveryApSsid);
  Serial.print("Recovery Wi-Fi password: ");
  Serial.println(recoveryApPassword);
  Serial.println("Recovery admin username: admin");
  Serial.print("Recovery admin password: ");
  Serial.println(recoveryAdminPassword);
  Serial.println(
    "Open http://192.168.50.1 and set new credentials."
  );
  Serial.println(
    "======================================="
  );
}

String jsonEscape(const String& value) {
  return RangeLinkText::jsonEscape(value);
}

String logKey(size_t index) {
  return "l" + String(index);
}

String wifiKey(size_t index, const char* suffix) {
  return "w" + String(index) + suffix;
}

String clientKey(size_t index, const char* suffix) {
  return "c" + String(index) + suffix;
}

void writeWifiSlot(size_t index, const WifiProfile& profile) {
  prefs.putString(wifiKey(index, "s").c_str(), profile.ssid);
  prefs.putBool(
    wifiKey(index, "o").c_str(),
    profile.openNetwork
  );

  if (profile.openNetwork) {
    prefs.remove(
      wifiKey(index, "p").c_str()
    );
  } else {
    const String protectedSecret =
      protectSecret(profile.secret);

    if (protectedSecret.length() > 0) {
      prefs.putString(
        wifiKey(index, "p").c_str(),
        protectedSecret
      );
    }
  }

  prefs.putInt(wifiKey(index, "q").c_str(), profile.priority);
  prefs.putInt(wifiKey(index, "r").c_str(), profile.lastRssi);
  prefs.putBool(wifiKey(index, "e").c_str(), profile.enabled);
  prefs.putULong64(wifiKey(index, "rx").c_str(), profile.rxBytes);
  prefs.putULong64(wifiKey(index, "tx").c_str(), profile.txBytes);
}

void clearWifiSlot(size_t index) {
  prefs.remove(wifiKey(index, "s").c_str());
  prefs.remove(wifiKey(index, "p").c_str());
  prefs.remove(wifiKey(index, "o").c_str());
  prefs.remove(wifiKey(index, "q").c_str());
  prefs.remove(wifiKey(index, "r").c_str());
  prefs.remove(wifiKey(index, "e").c_str());
  prefs.remove(wifiKey(index, "rx").c_str());
  prefs.remove(wifiKey(index, "tx").c_str());
}

void clearClientSlot(size_t index) {
  const char* suffixes[] = {
    "m","n","a","b","rx","tx","dr","dt","q",
    "mr","mt","mq","bw","gu","day","mon","se","sh","eh"
  };

  for (const char* suffix : suffixes) {
    prefs.remove(clientKey(index, suffix).c_str());
  }
}

bool writeClientSlot(size_t index, const ClientRecord& record) {
  bool ok = true;

  ok &= prefs.putString(clientKey(index, "m").c_str(), record.mac) > 0;
  // Empty device names are valid; Preferences returns 0 for an empty string.
  if (record.hostname.length()) {
    ok &= prefs.putString(clientKey(index, "n").c_str(), record.hostname) > 0;
  } else {
    prefs.remove(clientKey(index, "n").c_str());
  }

  ok &= prefs.putBool(clientKey(index, "a").c_str(), record.approved) > 0;
  ok &= prefs.putBool(clientKey(index, "b").c_str(), record.blocked) > 0;

  ok &= prefs.putULong64(clientKey(index, "rx").c_str(), record.rxBytes) > 0;
  ok &= prefs.putULong64(clientKey(index, "tx").c_str(), record.txBytes) > 0;
  ok &= prefs.putULong64(clientKey(index, "dr").c_str(), record.dailyRxBytes) > 0;
  ok &= prefs.putULong64(clientKey(index, "dt").c_str(), record.dailyTxBytes) > 0;
  ok &= prefs.putULong64(clientKey(index, "q").c_str(), record.dailyQuotaBytes) > 0;

  ok &= prefs.putULong64(clientKey(index, "mr").c_str(), record.monthlyRxBytes) > 0;
  ok &= prefs.putULong64(clientKey(index, "mt").c_str(), record.monthlyTxBytes) > 0;
  ok &= prefs.putULong64(clientKey(index, "mq").c_str(), record.monthlyQuotaBytes) > 0;

  ok &= prefs.putUInt(clientKey(index, "bw").c_str(), record.bandwidthKbps) > 0;
  ok &= prefs.putUInt(clientKey(index, "gu").c_str(), record.guestUntilEpoch) > 0;
  ok &= prefs.putInt(clientKey(index, "day").c_str(), record.usageDay) > 0;
  ok &= prefs.putInt(clientKey(index, "mon").c_str(), record.usageMonth) > 0;

  ok &= prefs.putBool(clientKey(index, "se").c_str(), record.scheduleEnabled) > 0;
  ok &= prefs.putUChar(clientKey(index, "sh").c_str(), record.scheduleStartHour) > 0;
  ok &= prefs.putUChar(clientKey(index, "eh").c_str(), record.scheduleEndHour) > 0;

  return ok;
}
}

void storageBegin() {
  prefs.begin("rangelink32", false);

  bootSequence = prefs.getUInt("boot_seq", 0) + 1;
  prefs.putUInt("boot_seq", bootSequence);

  if (!prefs.isKey("ap_ssid")) {
    prefs.putString("ap_ssid", RangeLinkConfig::DEFAULT_AP_SSID);
  }
  if (!prefs.isKey("ap_pass")) {
    prefs.putString(
      "ap_pass",
      protectSecret(
        RangeLinkConfig::DEFAULT_AP_PASSWORD
      )
    );
  } else {
    const String existing =
      prefs.getString("ap_pass", "");

    if (existing.length() > 0) {
      const bool legacyEncrypted =
        existing.startsWith("enc1:");

      if (
        !isProtectedSecret(existing) ||
        legacyEncrypted
      ) {
        const String plain =
          unprotectSecret(existing);

        if (plain.length() > 0) {
          prefs.putString(
            "ap_pass",
            protectSecret(plain)
          );
        }
      }
    }
  }
  if (!prefs.isKey("admin_user")) {
    prefs.putString("admin_user", RangeLinkConfig::DEFAULT_ADMIN_USER);
  }
  if (!prefs.isKey("admin_pass")) {
    prefs.putString(
      "admin_pass",
      protectSecret(
        RangeLinkConfig::DEFAULT_ADMIN_PASSWORD
      )
    );
  } else {
    const String existing =
      prefs.getString("admin_pass", "");

    if (existing.length() > 0) {
      const bool legacyEncrypted =
        existing.startsWith("enc1:");

      if (
        !isProtectedSecret(existing) ||
        legacyEncrypted
      ) {
        const String plain =
          unprotectSecret(existing);

        if (plain.length() > 0) {
          prefs.putString(
            "admin_pass",
            protectSecret(plain)
          );
        }
      }
    }
  }
  if (!prefs.isKey("tz_min")) {
    prefs.putInt("tz_min", 300);
  }
  if (!prefs.isKey("dns")) {
    prefs.putString("dns", "");
  }

  // Migrate any legacy plaintext saved upstream credentials.
  const uint8_t wifiCount =
    prefs.getUChar("wifi_n", 0);

  for (
    uint8_t i = 0;
    i < wifiCount &&
    i < RangeLinkConfig::MAX_WIFI_PROFILES;
    ++i
  ) {
    const String key =
      wifiKey(i, "p");

    const String existing =
      prefs.getString(key.c_str(), "");

    if (existing.length() > 0) {
      const bool legacyEncrypted =
        existing.startsWith("enc1:");

      if (
        !isProtectedSecret(existing) ||
        legacyEncrypted
      ) {
        const String plain =
          unprotectSecret(existing);

        if (plain.length() > 0) {
          prefs.putString(
            key.c_str(),
            protectSecret(plain)
          );
        }
      }
    }
  }

  const String storedAp =
    prefs.getString("ap_pass", "");

  const String storedAdmin =
    prefs.getString("admin_pass", "");

  if (
    storedAp.length() == 0 ||
    storedAdmin.length() == 0 ||
    unprotectSecret(storedAp).length() == 0 ||
    unprotectSecret(storedAdmin).length() == 0
  ) {
    enterCredentialRecoveryMode(
      "Stored hotspot/admin credentials could not be decrypted safely."
    );
  }
}


String getApSsid() {
  if (credentialRecoveryMode) {
    ensureRecoveryCredentials();
    return recoveryApSsid;
  }

  return prefs.getString(
    "ap_ssid",
    RangeLinkConfig::DEFAULT_AP_SSID
  );
}

String getApPassword() {
  if (credentialRecoveryMode) {
    ensureRecoveryCredentials();
    return recoveryApPassword;
  }

  const String stored =
    prefs.getString("ap_pass", "");

  const String plain =
    unprotectSecret(stored);

  if (plain.length()) return plain;

  enterCredentialRecoveryMode(
    "Hotspot credential decryption failed."
  );
  return recoveryApPassword;
}

String getAdminUser() {
  if (credentialRecoveryMode) {
    return "admin";
  }

  return prefs.getString(
    "admin_user",
    RangeLinkConfig::DEFAULT_ADMIN_USER
  );
}

String getAdminPassword() {
  if (credentialRecoveryMode) {
    ensureRecoveryCredentials();
    return recoveryAdminPassword;
  }

  const String stored =
    prefs.getString("admin_pass", "");

  const String plain =
    unprotectSecret(stored);

  if (plain.length()) return plain;

  enterCredentialRecoveryMode(
    "Administrator credential decryption failed."
  );
  return recoveryAdminPassword;
}

bool credentialRecoveryRequired() {
  return credentialRecoveryMode;
}

bool setApCredentials(const String& ssid, const String& password) {
  if (
    ssid.length() == 0 ||
    ssid.length() > 32 ||
    password.length() < 8 ||
    password.length() > 63
  ) {
    return false;
  }

  // Keep the Wi-Fi access password separate from the admin login password.
  if (password == getAdminPassword()) return false;

  const String protectedPassword =
    protectSecret(password);

  if (protectedPassword.length() == 0) {
    return false;
  }

  prefs.putString("ap_ssid", ssid);
  prefs.putString(
    "ap_pass",
    protectedPassword
  );

  return true;
}

bool setAdminCredentials(const String& username, const String& password) {
  if (
    username.length() == 0 ||
    username.length() > 32 ||
    password.length() < 8 ||
    password.length() > 64
  ) {
    return false;
  }

  // Do not allow the management password to match the Wi-Fi password.
  if (password == getApPassword()) return false;

  const String protectedPassword =
    protectSecret(password);

  if (protectedPassword.length() == 0) {
    return false;
  }

  prefs.putString("admin_user", username);
  prefs.putString(
    "admin_pass",
    protectedPassword
  );

  return true;
}


bool initialSetupRequired() {
  if (credentialRecoveryMode) return true;

  return
    getApPassword() ==
      RangeLinkConfig::DEFAULT_AP_PASSWORD ||
    getAdminPassword() ==
      RangeLinkConfig::DEFAULT_ADMIN_PASSWORD;
}

bool setInitialCredentials(
  const String& ssid,
  const String& apPassword,
  const String& adminUser,
  const String& adminPassword
) {
  if (
    ssid.length() == 0 ||
    ssid.length() > 32 ||
    apPassword.length() < 8 ||
    apPassword.length() > 63 ||
    adminUser.length() == 0 ||
    adminUser.length() > 32 ||
    adminPassword.length() < 8 ||
    adminPassword.length() > 64 ||
    apPassword == adminPassword ||
    apPassword ==
      RangeLinkConfig::DEFAULT_AP_PASSWORD ||
    adminPassword ==
      RangeLinkConfig::DEFAULT_ADMIN_PASSWORD
  ) {
    return false;
  }

  const String protectedAp =
    protectSecret(apPassword);
  const String protectedAdmin =
    protectSecret(adminPassword);

  if (
    protectedAp.length() == 0 ||
    protectedAdmin.length() == 0
  ) {
    return false;
  }

  prefs.putString("ap_ssid", ssid);
  prefs.putString("ap_pass", protectedAp);
  prefs.putString("admin_user", adminUser);
  prefs.putString("admin_pass", protectedAdmin);

  credentialRecoveryMode = false;
  recoveryApSsid = "";
  recoveryApPassword = "";
  recoveryAdminPassword = "";

  return true;
}

size_t loadWifiProfiles(WifiProfile* out, size_t maxCount) {
  if (!out || maxCount == 0) return 0;

  size_t count = prefs.getUChar("wifi_n", 0);
  if (count > RangeLinkConfig::MAX_WIFI_PROFILES) {
    count = RangeLinkConfig::MAX_WIFI_PROFILES;
  }
  if (count > maxCount) count = maxCount;

  size_t written = 0;

  for (size_t i = 0; i < count; ++i) {
    WifiProfile profile;

    profile.ssid =
      prefs.getString(wifiKey(i, "s").c_str(), "");
    profile.openNetwork =
      prefs.getBool(
        wifiKey(i, "o").c_str(),
        false
      );

    profile.secret =
      profile.openNetwork
        ? ""
        : unprotectSecret(
            prefs.getString(
              wifiKey(i, "p").c_str(),
              ""
            )
          );
    profile.priority =
      prefs.getInt(wifiKey(i, "q").c_str(), 100);
    profile.lastRssi =
      prefs.getInt(wifiKey(i, "r").c_str(), -127);
    profile.enabled =
      prefs.getBool(wifiKey(i, "e").c_str(), true);
    profile.rxBytes =
      prefs.getULong64(wifiKey(i, "rx").c_str(), 0);
    profile.txBytes =
      prefs.getULong64(wifiKey(i, "tx").c_str(), 0);

    if (profile.ssid.length() == 0) continue;
    out[written++] = profile;
  }

  return written;
}

bool saveWifiProfile(const WifiProfile& profile) {
  if (
    profile.ssid.length() == 0 ||
    profile.ssid.length() > 32 ||
    (!profile.openNetwork &&
      (profile.secret.length() < 8 ||
       profile.secret.length() > 63))
  ) {
    return false;
  }

  WifiProfile profiles[RangeLinkConfig::MAX_WIFI_PROFILES];
  size_t count =
    loadWifiProfiles(profiles, RangeLinkConfig::MAX_WIFI_PROFILES);

  for (size_t i = 0; i < count; ++i) {
    if (profiles[i].ssid == profile.ssid) {
      WifiProfile updated = profile;
      updated.priority = profiles[i].priority;
      updated.rxBytes = profiles[i].rxBytes;
      updated.txBytes = profiles[i].txBytes;
      writeWifiSlot(i, updated);
      return true;
    }
  }

  if (count >= RangeLinkConfig::MAX_WIFI_PROFILES) return false;

  WifiProfile stored = profile;
  stored.priority = 100 + static_cast<int>(count);

  writeWifiSlot(count, stored);
  prefs.putUChar("wifi_n", static_cast<uint8_t>(count + 1));

  return true;
}

bool getWifiProfileSecret(
  const String& ssid,
  String& secret
) {
  WifiProfile profiles[
    RangeLinkConfig::MAX_WIFI_PROFILES
  ];

  const size_t count =
    loadWifiProfiles(
      profiles,
      RangeLinkConfig::MAX_WIFI_PROFILES
    );

  for (size_t i = 0; i < count; ++i) {
    if (profiles[i].ssid == ssid) {
      secret = profiles[i].secret;
      return true;
    }
  }

  return false;
}

bool updateWifiProfileUsage(
  const String& ssid,
  int32_t rssi,
  uint64_t rxDelta,
  uint64_t txDelta
) {
  const size_t count =
    prefs.getUChar("wifi_n", 0);

  for (
    size_t i = 0;
    i < count &&
    i < RangeLinkConfig::MAX_WIFI_PROFILES;
    ++i
  ) {
    if (
      prefs.getString(
        wifiKey(i, "s").c_str(),
        ""
      ) == ssid
    ) {
      const uint64_t rx =
        prefs.getULong64(
          wifiKey(i, "rx").c_str(),
          0
        );
      const uint64_t tx =
        prefs.getULong64(
          wifiKey(i, "tx").c_str(),
          0
        );

      prefs.putULong64(
        wifiKey(i, "rx").c_str(),
        rx + rxDelta
      );
      prefs.putULong64(
        wifiKey(i, "tx").c_str(),
        tx + txDelta
      );
      prefs.putInt(
        wifiKey(i, "r").c_str(),
        rssi
      );

      return true;
    }
  }

  return false;
}

bool setWifiProfilePriority(
  const String& ssid,
  int priority
) {
  if (
    ssid.length() == 0 ||
    priority < 1 ||
    priority > 9999
  ) {
    return false;
  }

  const size_t count =
    prefs.getUChar("wifi_n", 0);

  for (
    size_t i = 0;
    i < count &&
    i < RangeLinkConfig::MAX_WIFI_PROFILES;
    ++i
  ) {
    if (
      prefs.getString(
        wifiKey(i, "s").c_str(),
        ""
      ) == ssid
    ) {
      prefs.putInt(
        wifiKey(i, "q").c_str(),
        priority
      );
      return true;
    }
  }

  return false;
}

bool removeWifiProfile(const String& ssid) {
  WifiProfile profiles[RangeLinkConfig::MAX_WIFI_PROFILES];
  size_t count =
    loadWifiProfiles(profiles, RangeLinkConfig::MAX_WIFI_PROFILES);

  size_t found = count;

  for (size_t i = 0; i < count; ++i) {
    if (profiles[i].ssid == ssid) {
      found = i;
      break;
    }
  }

  if (found == count) return false;

  for (size_t i = found; i + 1 < count; ++i) {
    writeWifiSlot(i, profiles[i + 1]);
  }

  if (count > 0) {
    clearWifiSlot(count - 1);
    prefs.putUChar("wifi_n", static_cast<uint8_t>(count - 1));
  }

  return true;
}

uint8_t getStoredAccessMode() {
  return prefs.getUChar("access_mode", 0);
}

void setStoredAccessMode(uint8_t mode) {
  prefs.putUChar("access_mode", mode);
}

size_t loadClientPolicies(ClientRecord* out, size_t maxCount) {
  if (!out || maxCount == 0) return 0;

  size_t count = prefs.getUChar("client_n", 0);

  if (count > RangeLinkConfig::MAX_CLIENT_RECORDS) {
    count = RangeLinkConfig::MAX_CLIENT_RECORDS;
  }
  if (count > maxCount) count = maxCount;

  size_t written = 0;

  for (size_t i = 0; i < count; ++i) {
    ClientRecord record;

    record.mac =
      prefs.getString(clientKey(i, "m").c_str(), "");
    record.hostname =
      prefs.getString(clientKey(i, "n").c_str(), "");
    record.approved =
      prefs.getBool(clientKey(i, "a").c_str(), false);
    record.blocked =
      prefs.getBool(clientKey(i, "b").c_str(), false);

    record.rxBytes =
      prefs.getULong64(clientKey(i, "rx").c_str(), 0);
    record.txBytes =
      prefs.getULong64(clientKey(i, "tx").c_str(), 0);
    record.dailyRxBytes =
      prefs.getULong64(clientKey(i, "dr").c_str(), 0);
    record.dailyTxBytes =
      prefs.getULong64(clientKey(i, "dt").c_str(), 0);
    record.dailyQuotaBytes =
      prefs.getULong64(clientKey(i, "q").c_str(), 0);

    record.monthlyRxBytes =
      prefs.getULong64(clientKey(i, "mr").c_str(), 0);
    record.monthlyTxBytes =
      prefs.getULong64(clientKey(i, "mt").c_str(), 0);
    record.monthlyQuotaBytes =
      prefs.getULong64(clientKey(i, "mq").c_str(), 0);

    record.bandwidthKbps =
      prefs.getUInt(clientKey(i, "bw").c_str(), 0);
    record.guestUntilEpoch =
      prefs.getUInt(clientKey(i, "gu").c_str(), 0);
    record.usageDay =
      prefs.getInt(clientKey(i, "day").c_str(), -1);
    record.usageMonth =
      prefs.getInt(clientKey(i, "mon").c_str(), -1);

    record.scheduleEnabled =
      prefs.getBool(clientKey(i, "se").c_str(), false);
    record.scheduleStartHour =
      prefs.getUChar(clientKey(i, "sh").c_str(), 0);
    record.scheduleEndHour =
      prefs.getUChar(clientKey(i, "eh").c_str(), 24);

    if (record.mac.length() == 0) continue;
    out[written++] = record;
  }

  return written;
}

bool saveClientPolicy(const ClientRecord& record) {
  if (record.mac.length() != 17) return false;

  ClientRecord policies[RangeLinkConfig::MAX_CLIENT_RECORDS];
  size_t count =
    loadClientPolicies(policies, RangeLinkConfig::MAX_CLIENT_RECORDS);

  for (size_t i = 0; i < count; ++i) {
    if (policies[i].mac.equalsIgnoreCase(record.mac)) {
      writeClientSlot(i, record);
      return true;
    }
  }

  if (count >= RangeLinkConfig::MAX_CLIENT_RECORDS) return false;

  writeClientSlot(count, record);
  prefs.putUChar("client_n", static_cast<uint8_t>(count + 1));

  return true;
}


bool removeClientPolicy(const String& mac) {
  ClientRecord policies[RangeLinkConfig::MAX_CLIENT_RECORDS];
  size_t count =
    loadClientPolicies(
      policies,
      RangeLinkConfig::MAX_CLIENT_RECORDS
    );

  size_t found = count;

  for (size_t i = 0; i < count; ++i) {
    if (policies[i].mac.equalsIgnoreCase(mac)) {
      found = i;
      break;
    }
  }

  if (found == count) return false;

  for (size_t i = found; i + 1 < count; ++i) {
    writeClientSlot(i, policies[i + 1]);
  }

  if (count > 0) {
    clearClientSlot(count - 1);
    prefs.putUChar(
      "client_n",
      static_cast<uint8_t>(count - 1)
    );
  }

  return true;
}

int getTimezoneOffsetMinutes() {
  return prefs.getInt("tz_min", 300);
}

void setTimezoneOffsetMinutes(int minutes) {
  if (minutes < -720) minutes = -720;
  if (minutes > 840) minutes = 840;
  prefs.putInt("tz_min", minutes);
}

String getCustomDns() {
  return prefs.getString("dns", "");
}

bool setCustomDns(const String& dns) {
  if (dns.length() == 0) {
    prefs.putString("dns", "");
    return true;
  }

  IPAddress parsed;
  if (!parsed.fromString(dns)) return false;

  prefs.putString("dns", dns);
  return true;
}

void appendEventLog(const String& type, const String& message) {
  uint8_t head = prefs.getUChar("log_head", 0);
  uint8_t count = prefs.getUChar("log_count", 0);

  String safeType = type;
  String safeMessage = message;

  safeType.replace("|", "/");
  safeMessage.replace("|", "/");
  safeMessage.replace("\n", " ");
  safeMessage.replace("\r", " ");

  String entry =
    String(bootSequence) + "|" +
    String(millis() / 1000UL) + "|" +
    safeType + "|" +
    safeMessage;

  prefs.putString(logKey(head).c_str(), entry);

  head =
    static_cast<uint8_t>((head + 1) % MAX_EVENT_LOGS);

  if (count < MAX_EVENT_LOGS) ++count;

  prefs.putUChar("log_head", head);
  prefs.putUChar("log_count", count);
}

String getEventLogJson() {
  const uint8_t head =
    prefs.getUChar("log_head", 0);
  const uint8_t count =
    prefs.getUChar("log_count", 0);

  String json = "[";

  const uint8_t oldest =
    static_cast<uint8_t>(
      (head + MAX_EVENT_LOGS - count) % MAX_EVENT_LOGS
    );

  for (uint8_t n = 0; n < count; ++n) {
    const uint8_t index =
      static_cast<uint8_t>((oldest + n) % MAX_EVENT_LOGS);

    String entry =
      prefs.getString(logKey(index).c_str(), "");

    if (entry.length() == 0) continue;

    const int p1 = entry.indexOf('|');
    const int p2 = entry.indexOf('|', p1 + 1);
    const int p3 = entry.indexOf('|', p2 + 1);

    if (p1 < 0 || p2 < 0 || p3 < 0) continue;

    if (json.length() > 1) json += ",";

    const String boot = entry.substring(0, p1);
    const String seconds = entry.substring(p1 + 1, p2);
    const String type = entry.substring(p2 + 1, p3);
    const String message = entry.substring(p3 + 1);

    json += "{\"boot\":" + boot +
            ",\"seconds\":" + seconds +
            ",\"type\":\"" + jsonEscape(type) +
            "\",\"message\":\"" + jsonEscape(message) +
            "\"}";
  }

  json += "]";
  return json;
}

void clearEventLogs() {
  for (uint8_t i = 0; i < MAX_EVENT_LOGS; ++i) {
    prefs.remove(logKey(i).c_str());
  }

  prefs.putUChar("log_head", 0);
  prefs.putUChar("log_count", 0);
}

String exportSafeSettings() {
  String output;

  output.reserve(4096);

  output += "RANGELINK32_BACKUP_V2\n";
  output += "ap_ssid=" + getApSsid() + "\n";
  output += "admin_user=" + getAdminUser() + "\n";
  output += "timezone_minutes=" +
            String(
              getTimezoneOffsetMinutes()
            ) + "\n";
  output += "custom_dns=" +
            getCustomDns() + "\n";
  output += "access_mode=" +
            String(
              getStoredAccessMode()
            ) + "\n";

  ClientRecord clients[
    RangeLinkConfig::MAX_CLIENT_RECORDS
  ];

  const size_t count =
    loadClientPolicies(
      clients,
      RangeLinkConfig::MAX_CLIENT_RECORDS
    );

  for (size_t i = 0; i < count; ++i) {
    String safeName = clients[i].hostname;
    safeName.replace("\n", " ");
    safeName.replace("\r", " ");
    safeName.replace("|", "/");

    output +=
      "client=" +
      clients[i].mac + "|" +
      safeName + "|" +
      String(clients[i].approved ? 1 : 0) + "|" +
      String(clients[i].blocked ? 1 : 0) + "|" +
      String(
        static_cast<unsigned long long>(
          clients[i].dailyQuotaBytes
        )
      ) + "|" +
      String(
        static_cast<unsigned long long>(
          clients[i].monthlyQuotaBytes
        )
      ) + "|" +
      String(clients[i].bandwidthKbps) + "|" +
      String(
        clients[i].scheduleEnabled
          ? 1
          : 0
      ) + "|" +
      String(clients[i].scheduleStartHour) + "|" +
      String(clients[i].scheduleEndHour) +
      "\n";
  }

  output +=
    "# Wi-Fi passwords and admin password are intentionally not exported.\n";

  return output;
}

namespace {

bool parseLongStrict(
  const String& value,
  long minimum,
  long maximum,
  long& output
) {
  if (value.length() == 0) return false;

  errno = 0;
  char* end = nullptr;
  const long parsed =
    strtol(value.c_str(), &end, 10);

  if (
    errno == ERANGE ||
    end == value.c_str() ||
    !end ||
    *end != '\0' ||
    parsed < minimum ||
    parsed > maximum
  ) {
    return false;
  }

  output = parsed;
  return true;
}

bool parseUint64Strict(
  const String& value,
  uint64_t maximum,
  uint64_t& output
) {
  if (
    value.length() == 0 ||
    value[0] == '-'
  ) {
    return false;
  }

  errno = 0;
  char* end = nullptr;

  const unsigned long long parsed =
    strtoull(
      value.c_str(),
      &end,
      10
    );

  if (
    errno == ERANGE ||
    end == value.c_str() ||
    !end ||
    *end != '\0' ||
    parsed > maximum
  ) {
    return false;
  }

  output = static_cast<uint64_t>(parsed);
  return true;
}

bool validMacText(const String& mac) {
  if (mac.length() != 17) return false;

  for (size_t i = 0; i < mac.length(); ++i) {
    const bool separator =
      i == 2 || i == 5 || i == 8 ||
      i == 11 || i == 14;

    if (separator) {
      if (mac[i] != ':') return false;
      continue;
    }

    const char c = mac[i];
    const bool hex =
      (c >= '0' && c <= '9') ||
      (c >= 'a' && c <= 'f') ||
      (c >= 'A' && c <= 'F');

    if (!hex) return false;
  }

  return true;
}

bool sameMac(
  const ClientRecord& a,
  const ClientRecord& b
) {
  return a.mac.equalsIgnoreCase(b.mac);
}

bool replaceClientPoliciesChecked(
  const ClientRecord* records,
  size_t count
) {
  if (
    count >
    RangeLinkConfig::MAX_CLIENT_RECORDS
  ) {
    return false;
  }

  for (
    size_t i = 0;
    i < RangeLinkConfig::MAX_CLIENT_RECORDS;
    ++i
  ) {
    clearClientSlot(i);
  }

  if (prefs.putUChar("client_n", 0) == 0) {
    return false;
  }

  for (size_t i = 0; i < count; ++i) {
    if (!writeClientSlot(i, records[i])) {
      return false;
    }
  }

  return
    prefs.putUChar(
      "client_n",
      static_cast<uint8_t>(count)
    ) > 0;
}

bool applyRestoredSettings(
  const String& apSsid,
  const String& adminUser,
  int timezoneMinutes,
  const String& customDns,
  uint8_t accessMode,
  const ClientRecord* clients,
  size_t clientCount
) {
  bool ok = true;

  ok &=
    prefs.putString(
      "ap_ssid",
      apSsid
    ) > 0;

  ok &=
    prefs.putString(
      "admin_user",
      adminUser
    ) > 0;

  ok &=
    prefs.putInt(
      "tz_min",
      timezoneMinutes
    ) > 0;

  if (customDns.length()) {
    ok &=
      prefs.putString(
        "dns",
        customDns
      ) > 0;
  } else {
    prefs.remove("dns");
  }

  ok &=
    prefs.putUChar(
      "access_mode",
      accessMode
    ) > 0;

  if (!ok) return false;

  return replaceClientPoliciesChecked(
    clients,
    clientCount
  );
}

}  // namespace

bool importSafeSettings(
  const String& text
) {
  constexpr size_t MAX_BACKUP_BYTES = 16384;
  constexpr size_t MAX_BACKUP_LINE = 512;

  if (
    text.length() == 0 ||
    text.length() > MAX_BACKUP_BYTES
  ) {
    return false;
  }

  const bool backupV1 =
    text.startsWith(
      "RANGELINK32_BACKUP_V1"
    );

  const bool backupV2 =
    text.startsWith(
      "RANGELINK32_BACKUP_V2"
    );

  if (!backupV1 && !backupV2) {
    return false;
  }

  // Stage everything in RAM first. No NVS state is touched
  // until the complete backup has passed validation.
  String stagedApSsid = getApSsid();
  String stagedAdminUser = getAdminUser();
  int stagedTimezone =
    getTimezoneOffsetMinutes();
  String stagedDns = getCustomDns();
  uint8_t stagedAccess =
    getStoredAccessMode();

  ClientRecord stagedClients[
    RangeLinkConfig::MAX_CLIENT_RECORDS
  ];
  size_t stagedClientCount = 0;

  int cursor = 0;

  while (cursor < text.length()) {
    int end = text.indexOf('\n', cursor);

    if (end < 0) end = text.length();

    if (
      end - cursor >
      static_cast<int>(MAX_BACKUP_LINE)
    ) {
      return false;
    }

    String line =
      text.substring(cursor, end);

    line.trim();
    cursor = end + 1;

    if (
      line.length() == 0 ||
      line.startsWith("#") ||
      line ==
        "RANGELINK32_BACKUP_V1" ||
      line ==
        "RANGELINK32_BACKUP_V2"
    ) {
      continue;
    }

    const int eq = line.indexOf('=');
    if (eq <= 0) return false;

    const String key =
      line.substring(0, eq);

    const String value =
      line.substring(eq + 1);

    if (key == "ap_ssid") {
      if (
        value.length() == 0 ||
        value.length() > 32
      ) {
        return false;
      }

      stagedApSsid = value;
      continue;
    }

    if (key == "admin_user") {
      if (
        value.length() == 0 ||
        value.length() > 32
      ) {
        return false;
      }

      stagedAdminUser = value;
      continue;
    }

    if (key == "timezone_minutes") {
      long parsed = 0;

      if (
        !parseLongStrict(
          value,
          -720,
          840,
          parsed
        )
      ) {
        return false;
      }

      stagedTimezone =
        static_cast<int>(parsed);
      continue;
    }

    if (key == "custom_dns") {
      if (value.length()) {
        IPAddress parsed;
        if (!parsed.fromString(value)) {
          return false;
        }
      }

      stagedDns = value;
      continue;
    }

    if (key == "access_mode") {
      long parsed = 0;

      if (
        !parseLongStrict(
          value,
          0,
          1,
          parsed
        )
      ) {
        return false;
      }

      stagedAccess =
        static_cast<uint8_t>(parsed);
      continue;
    }

    if (key != "client") {
      // Ignore future unknown keys, but never partially
      // apply a malformed known setting.
      continue;
    }

    if (
      stagedClientCount >=
      RangeLinkConfig::MAX_CLIENT_RECORDS
    ) {
      return false;
    }

    String parts[10];
    int partCount = 0;
    int start = 0;

    for (
      int i = 0;
      i <= value.length();
      ++i
    ) {
      if (
        i == value.length() ||
        value[i] == '|'
      ) {
        if (partCount >= 10) {
          return false;
        }

        parts[partCount++] =
          value.substring(start, i);
        start = i + 1;
      }
    }

    const int expectedParts =
      backupV2 ? 10 : 9;

    if (partCount != expectedParts) {
      return false;
    }

    ClientRecord record;
    record.mac = parts[0];

    if (!validMacText(record.mac)) {
      return false;
    }

    record.hostname = parts[1];

    if (record.hostname.length() > 32) {
      return false;
    }

    for (
      size_t i = 0;
      i < stagedClientCount;
      ++i
    ) {
      if (
        stagedClients[i].mac
          .equalsIgnoreCase(record.mac)
      ) {
        return false;
      }
    }

    long approved = 0;
    long blocked = 0;

    if (
      !parseLongStrict(parts[2], 0, 1, approved) ||
      !parseLongStrict(parts[3], 0, 1, blocked)
    ) {
      return false;
    }

    record.approved = approved == 1;
    record.blocked = blocked == 1;

    uint64_t dailyQuota = 0;
    if (
      !parseUint64Strict(
        parts[4],
        UINT64_MAX,
        dailyQuota
      )
    ) {
      return false;
    }

    record.dailyQuotaBytes =
      dailyQuota;

    const bool v2Client =
      backupV2;

    const int monthlyIndex =
      v2Client ? 5 : -1;
    const int bandwidthIndex =
      v2Client ? 6 : 5;
    const int enabledIndex =
      v2Client ? 7 : 6;
    const int startIndex =
      v2Client ? 8 : 7;
    const int endIndex =
      v2Client ? 9 : 8;

    if (v2Client) {
      uint64_t monthlyQuota = 0;

      if (
        !parseUint64Strict(
          parts[monthlyIndex],
          UINT64_MAX,
          monthlyQuota
        )
      ) {
        return false;
      }

      record.monthlyQuotaBytes =
        monthlyQuota;
    }

    uint64_t bandwidth = 0;
    if (
      !parseUint64Strict(
        parts[bandwidthIndex],
        UINT32_MAX,
        bandwidth
      )
    ) {
      return false;
    }

    record.bandwidthKbps =
      static_cast<uint32_t>(bandwidth);

    long enabled = 0;
    long startHour = 0;
    long endHour = 0;

    if (
      !parseLongStrict(
        parts[enabledIndex],
        0,
        1,
        enabled
      ) ||
      !parseLongStrict(
        parts[startIndex],
        0,
        23,
        startHour
      ) ||
      !parseLongStrict(
        parts[endIndex],
        0,
        24,
        endHour
      )
    ) {
      return false;
    }

    record.scheduleEnabled =
      enabled == 1;

    record.scheduleStartHour =
      static_cast<uint8_t>(startHour);

    record.scheduleEndHour =
      static_cast<uint8_t>(endHour);

    stagedClients[
      stagedClientCount++
    ] = record;
  }

  // Snapshot current restorable state for rollback.
  const String oldApSsid =
    prefs.getString(
      "ap_ssid",
      RangeLinkConfig::DEFAULT_AP_SSID
    );

  const String oldAdminUser =
    prefs.getString(
      "admin_user",
      RangeLinkConfig::DEFAULT_ADMIN_USER
    );

  const int oldTimezone =
    getTimezoneOffsetMinutes();

  const String oldDns =
    getCustomDns();

  const uint8_t oldAccess =
    getStoredAccessMode();

  ClientRecord oldClients[
    RangeLinkConfig::MAX_CLIENT_RECORDS
  ];

  const size_t oldClientCount =
    loadClientPolicies(
      oldClients,
      RangeLinkConfig::MAX_CLIENT_RECORDS
    );

  if (
    applyRestoredSettings(
      stagedApSsid,
      stagedAdminUser,
      stagedTimezone,
      stagedDns,
      stagedAccess,
      stagedClients,
      stagedClientCount
    )
  ) {
    return true;
  }

  // Best-effort rollback if NVS reports a write failure.
  applyRestoredSettings(
    oldApSsid,
    oldAdminUser,
    oldTimezone,
    oldDns,
    oldAccess,
    oldClients,
    oldClientCount
  );

  return false;
}

void factoryResetStorage() {
  prefs.clear();

  Preferences securePrefs;

  if (securePrefs.begin("rl32-sec", false)) {
    securePrefs.clear();
    securePrefs.end();
  }

  credentialRecoveryMode = false;
  recoveryApSsid = "";
  recoveryApPassword = "";
  recoveryAdminPassword = "";
}
