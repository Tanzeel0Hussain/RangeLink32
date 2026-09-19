#include <Preferences.h>
#include "storage.h"
#include "config.h"

namespace {
Preferences prefs;

String wifiKey(size_t index, const char* suffix) {
  return "w" + String(index) + suffix;
}

void writeWifiSlot(size_t index, const WifiProfile& profile) {
  prefs.putString(wifiKey(index, "s").c_str(), profile.ssid);
  prefs.putString(wifiKey(index, "p").c_str(), profile.secret);
  prefs.putInt(wifiKey(index, "q").c_str(), profile.priority);
  prefs.putInt(wifiKey(index, "r").c_str(), profile.lastRssi);
  prefs.putBool(wifiKey(index, "e").c_str(), profile.enabled);
}

void clearWifiSlot(size_t index) {
  prefs.remove(wifiKey(index, "s").c_str());
  prefs.remove(wifiKey(index, "p").c_str());
  prefs.remove(wifiKey(index, "q").c_str());
  prefs.remove(wifiKey(index, "r").c_str());
  prefs.remove(wifiKey(index, "e").c_str());
}
}

void storageBegin() {
  prefs.begin("rangelink32", false);

  if (!prefs.isKey("ap_ssid")) {
    prefs.putString("ap_ssid", RangeLinkConfig::DEFAULT_AP_SSID);
  }
  if (!prefs.isKey("ap_pass")) {
    prefs.putString("ap_pass", RangeLinkConfig::DEFAULT_AP_PASSWORD);
  }
  if (!prefs.isKey("admin_user")) {
    prefs.putString("admin_user", RangeLinkConfig::DEFAULT_ADMIN_USER);
  }
  if (!prefs.isKey("admin_pass")) {
    // Development bootstrap only. A later security milestone replaces this
    // with protected admin credentials and encrypted upstream secrets.
    prefs.putString("admin_pass", RangeLinkConfig::DEFAULT_ADMIN_PASSWORD);
  }
}

String getApSsid() {
  return prefs.getString("ap_ssid", RangeLinkConfig::DEFAULT_AP_SSID);
}

String getApPassword() {
  return prefs.getString("ap_pass", RangeLinkConfig::DEFAULT_AP_PASSWORD);
}

String getAdminUser() {
  return prefs.getString("admin_user", RangeLinkConfig::DEFAULT_ADMIN_USER);
}

String getAdminPassword() {
  return prefs.getString("admin_pass", RangeLinkConfig::DEFAULT_ADMIN_PASSWORD);
}

bool setApCredentials(const String& ssid, const String& password) {
  if (ssid.length() == 0 || password.length() < 8) return false;
  prefs.putString("ap_ssid", ssid);
  prefs.putString("ap_pass", password);
  return true;
}

bool setAdminCredentials(const String& username, const String& password) {
  if (username.length() == 0 || password.length() < 8) return false;
  prefs.putString("admin_user", username);
  prefs.putString("admin_pass", password);
  return true;
}

size_t loadWifiProfiles(WifiProfile* out, size_t maxCount) {
  if (!out || maxCount == 0) return 0;

  size_t count = prefs.getUChar("wifi_n", 0);
  if (count > RangeLinkConfig::MAX_WIFI_PROFILES) {
    count = RangeLinkConfig::MAX_WIFI_PROFILES;
  }
  if (count > maxCount) {
    count = maxCount;
  }

  size_t written = 0;
  for (size_t i = 0; i < count; ++i) {
    WifiProfile profile;
    profile.ssid = prefs.getString(wifiKey(i, "s").c_str(), "");
    profile.secret = prefs.getString(wifiKey(i, "p").c_str(), "");
    profile.priority = prefs.getInt(wifiKey(i, "q").c_str(), 100);
    profile.lastRssi = prefs.getInt(wifiKey(i, "r").c_str(), -127);
    profile.enabled = prefs.getBool(wifiKey(i, "e").c_str(), true);

    if (profile.ssid.length() == 0) continue;
    out[written++] = profile;
  }

  return written;
}

bool saveWifiProfile(const WifiProfile& profile) {
  if (profile.ssid.length() == 0 || profile.secret.length() < 8) return false;

  WifiProfile profiles[RangeLinkConfig::MAX_WIFI_PROFILES];
  size_t count = loadWifiProfiles(profiles, RangeLinkConfig::MAX_WIFI_PROFILES);

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

bool removeWifiProfile(const String& ssid) {
  WifiProfile profiles[RangeLinkConfig::MAX_WIFI_PROFILES];
  size_t count = loadWifiProfiles(profiles, RangeLinkConfig::MAX_WIFI_PROFILES);

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

void factoryResetStorage() {
  prefs.clear();
}
