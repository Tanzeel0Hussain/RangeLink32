#include <cassert>
#include <iostream>
#include "policy_logic.h"

int main() {
  using namespace RangeLinkLogic;

  // Schedules: disabled, daytime, overnight, equal-hours, unknown time.
  assert(scheduleAllowsHour(false, 8, 20, 3));
  assert(scheduleAllowsHour(true, 8, 20, 8));
  assert(scheduleAllowsHour(true, 8, 20, 19));
  assert(!scheduleAllowsHour(true, 8, 20, 20));
  assert(scheduleAllowsHour(true, 22, 6, 23));
  assert(scheduleAllowsHour(true, 22, 6, 5));
  assert(!scheduleAllowsHour(true, 22, 6, 12));
  assert(scheduleAllowsHour(true, 8, 8, 12));
  assert(!scheduleAllowsHour(true, 8, 20, 255));

  // Quotas: unlimited, below limit, exact boundary and next-packet overflow.
  assert(!quotaReached(100, 200, 0));
  assert(!quotaReached(100, 200, 301));
  assert(quotaReached(100, 200, 300));
  assert(quotaAllowsPacket(100, 200, 0, 5000));
  assert(quotaAllowsPacket(100, 200, 350, 50));
  assert(!quotaAllowsPacket(100, 200, 350, 51));

  // Upstream credential validation.
  assert(validUpstreamSecret(true, 0));
  assert(!validUpstreamSecret(false, 0));
  assert(!validUpstreamSecret(false, 7));
  assert(validUpstreamSecret(false, 8));
  assert(validUpstreamSecret(false, 63));
  assert(!validUpstreamSecret(false, 64));

  // Failover priority wins first; RSSI breaks equal-priority ties.
  assert(profileIsBetter(100, -80, false, 0, -127));
  assert(profileIsBetter(50, -90, true, 100, -40));
  assert(!profileIsBetter(150, -30, true, 100, -90));
  assert(profileIsBetter(100, -60, true, 100, -75));
  assert(!profileIsBetter(100, -85, true, 100, -75));

  std::cout << "RangeLink32 host logic tests passed\n";
  return 0;
}
