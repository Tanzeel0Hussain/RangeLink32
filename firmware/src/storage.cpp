#include <Preferences.h>
#include "storage.h"
#include "config.h"

namespace {
Preferences prefs;
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
    // with protected credential storage and re-authentication support.
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

void factoryResetStorage() {
  prefs.clear();
}
