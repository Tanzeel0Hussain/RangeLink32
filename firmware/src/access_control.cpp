#include "access_control.h"

namespace {
AccessMode mode = AccessMode::AllowAll;
}

void accessControlBegin() {}
void accessControlLoop() {}

AccessMode getAccessMode() {
  return mode;
}

void setAccessMode(AccessMode newMode) {
  mode = newMode;
}

bool setClientApproval(const String&, bool) {
  // Persistent client policy table is added in the next milestone.
  return true;
}

bool setClientBlocked(const String&, bool) {
  // Persistent client policy table is added in the next milestone.
  return true;
}

bool clientMayUseInternet(const String&) {
  return mode == AccessMode::AllowAll;
}

String getClientTableJson() {
  return "[]";
}
