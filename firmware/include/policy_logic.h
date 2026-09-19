#pragma once
#include <stddef.h>
#include <stdint.h>

namespace RangeLinkLogic {

inline bool scheduleAllowsHour(
  bool enabled,
  uint8_t startHour,
  uint8_t endHour,
  uint8_t currentHour
) {
  if (!enabled) return true;

  // 255 means time is not synchronized yet.
  if (currentHour == 255) return true;

  const uint8_t start =
    startHour > 23 ? 23 : startHour;

  const uint8_t end =
    endHour > 24 ? 24 : endHour;

  if (start == end) return true;

  if (start < end) {
    return
      currentHour >= start &&
      currentHour < end;
  }

  return
    currentHour >= start ||
    currentHour < end;
}

inline bool quotaReached(
  uint64_t rxBytes,
  uint64_t txBytes,
  uint64_t quotaBytes
) {
  return
    quotaBytes > 0 &&
    rxBytes + txBytes >= quotaBytes;
}

inline bool quotaAllowsPacket(
  uint64_t rxBytes,
  uint64_t txBytes,
  uint64_t quotaBytes,
  uint64_t packetBytes
) {
  return
    quotaBytes == 0 ||
    rxBytes + txBytes + packetBytes <=
      quotaBytes;
}

inline bool validUpstreamSecret(
  bool openNetwork,
  size_t passwordLength
) {
  return
    openNetwork ||
    (passwordLength >= 8 &&
     passwordLength <= 63);
}

inline bool profileIsBetter(
  int candidatePriority,
  int32_t candidateRssi,
  bool haveCurrentBest,
  int currentPriority,
  int32_t currentRssi
) {
  return
    !haveCurrentBest ||
    candidatePriority < currentPriority ||
    (
      candidatePriority == currentPriority &&
      candidateRssi > currentRssi
    );
}

}  // namespace RangeLinkLogic
