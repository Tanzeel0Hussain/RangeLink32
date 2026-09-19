#include <Arduino.h>
#include <cstring>

extern "C" {
#include "esp_netif.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/prot/ip4.h"

struct netif* esp_netif_get_netif_impl(
  esp_netif_t* esp_netif
);
}

#include "traffic_monitor.h"
#include "config.h"

namespace {
struct TrafficSlot {
  uint8_t mac[6] = {};
  bool active = false;
  bool internetAllowed = true;

  uint64_t rxBytes = 0;
  uint64_t txBytes = 0;
  uint64_t dailyRxBytes = 0;
  uint64_t dailyTxBytes = 0;
  uint64_t dailyQuotaBytes = 0;

  uint32_t bandwidthKbps = 0;
  uint32_t rateWindowStartMs = 0;
  uint32_t rateWindowBytes = 0;
};

TrafficSlot slots[RangeLinkConfig::MAX_CLIENT_RECORDS];
bool defaultAllow = true;

struct netif* apNetif = nullptr;
netif_input_fn originalApInput = nullptr;
netif_linkoutput_fn originalApLinkOutput = nullptr;

portMUX_TYPE trafficMux = portMUX_INITIALIZER_UNLOCKED;

bool parseMac(const String& text, uint8_t out[6]) {
  if (text.length() != 17) return false;

  unsigned int b[6] = {};
  if (sscanf(
        text.c_str(),
        "%02x:%02x:%02x:%02x:%02x:%02x",
        &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]
      ) != 6) {
    return false;
  }

  for (int i = 0; i < 6; ++i) {
    out[i] = static_cast<uint8_t>(b[i]);
  }
  return true;
}

int findSlotByMac(const uint8_t mac[6]) {
  for (size_t i = 0; i < RangeLinkConfig::MAX_CLIENT_RECORDS; ++i) {
    if (slots[i].active && memcmp(slots[i].mac, mac, 6) == 0) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

int findOrCreateSlot(const uint8_t mac[6]) {
  const int existing = findSlotByMac(mac);
  if (existing >= 0) return existing;

  for (size_t i = 0; i < RangeLinkConfig::MAX_CLIENT_RECORDS; ++i) {
    if (!slots[i].active) {
      slots[i] = TrafficSlot();
      memcpy(slots[i].mac, mac, 6);
      slots[i].active = true;
      slots[i].internetAllowed = defaultAllow;
      slots[i].rateWindowStartMs = millis();
      return static_cast<int>(i);
    }
  }

  return -1;
}

bool isLocalOrInfrastructureIPv4(const uint8_t* frame, size_t len) {
  if (!frame || len < 14 + sizeof(struct ip_hdr)) return true;

  const uint16_t etherType =
    (static_cast<uint16_t>(frame[12]) << 8) | frame[13];

  if (etherType == 0x0806) {
    // ARP is always local/infrastructure traffic.
    return true;
  }

  if (etherType != 0x0800) {
    // Non-IPv4 traffic is not routed by our IPv4 NAPT engine.
    return true;
  }

  const struct ip_hdr* ip =
    reinterpret_cast<const struct ip_hdr*>(frame + 14);

  if (IPH_V(ip) != 4) return true;

  const uint32_t dest = lwip_ntohl(ip->dest.addr);

  if (dest == 0xFFFFFFFFUL) return true;
  if ((dest & 0xFFFFFF00UL) == 0xC0A83200UL) return true; // 192.168.50.0/24
  if ((dest & 0xF0000000UL) == 0xE0000000UL) return true; // multicast

  return false;
}

bool isInternetDownlink(const uint8_t* frame, size_t len) {
  if (!frame || len < 14 + sizeof(struct ip_hdr)) return false;

  const uint16_t etherType =
    (static_cast<uint16_t>(frame[12]) << 8) | frame[13];

  if (etherType != 0x0800) return false;

  const struct ip_hdr* ip =
    reinterpret_cast<const struct ip_hdr*>(frame + 14);

  if (IPH_V(ip) != 4) return false;

  const uint32_t src = lwip_ntohl(ip->src.addr);

  if ((src & 0xFFFFFF00UL) == 0xC0A83200UL) return false;
  if ((src & 0xF0000000UL) == 0xE0000000UL) return false;
  if (src == 0 || src == 0xFFFFFFFFUL) return false;

  return true;
}

bool consumeBudget(TrafficSlot& slot, uint16_t bytes, bool downlink) {
  if (!slot.internetAllowed) return false;

  const uint64_t dailyTotal =
    slot.dailyRxBytes + slot.dailyTxBytes;

  if (
    slot.dailyQuotaBytes > 0 &&
    dailyTotal + bytes > slot.dailyQuotaBytes
  ) {
    return false;
  }

  if (slot.bandwidthKbps > 0) {
    const uint32_t now = millis();

    if (now - slot.rateWindowStartMs >= 1000UL) {
      slot.rateWindowStartMs = now;
      slot.rateWindowBytes = 0;
    }

    const uint64_t maxBytesPerSecond =
      (static_cast<uint64_t>(slot.bandwidthKbps) * 1000ULL) / 8ULL;

    if (
      maxBytesPerSecond > 0 &&
      static_cast<uint64_t>(slot.rateWindowBytes) + bytes >
        maxBytesPerSecond
    ) {
      return false;
    }

    slot.rateWindowBytes += bytes;
  }

  if (downlink) {
    slot.rxBytes += bytes;
    slot.dailyRxBytes += bytes;
  } else {
    slot.txBytes += bytes;
    slot.dailyTxBytes += bytes;
  }

  return true;
}

err_t apInputHook(struct pbuf* p, struct netif* netif) {
  if (p && p->len >= 14) {
    const uint8_t* frame =
      static_cast<const uint8_t*>(p->payload);

    const uint8_t* srcMac = frame + 6;
    const bool internetTraffic =
      !isLocalOrInfrastructureIPv4(frame, p->len);

    if (internetTraffic) {
      bool permitted = defaultAllow;

      portENTER_CRITICAL(&trafficMux);
      const int index = findSlotByMac(srcMac);

      if (index >= 0) {
        permitted = consumeBudget(
          slots[index],
          static_cast<uint16_t>(p->tot_len),
          false
        );
      }
      portEXIT_CRITICAL(&trafficMux);

      if (!permitted) {
        pbuf_free(p);
        return ERR_OK;
      }
    }
  }

  return originalApInput
    ? originalApInput(p, netif)
    : ERR_VAL;
}

err_t apLinkOutputHook(struct netif* netif, struct pbuf* p) {
  if (p && p->len >= 14) {
    const uint8_t* frame =
      static_cast<const uint8_t*>(p->payload);

    const uint8_t* dstMac = frame;
    const bool internetTraffic =
      isInternetDownlink(frame, p->len);

    if (internetTraffic) {
      bool permitted = defaultAllow;

      portENTER_CRITICAL(&trafficMux);
      const int index = findSlotByMac(dstMac);

      if (index >= 0) {
        permitted = consumeBudget(
          slots[index],
          static_cast<uint16_t>(p->tot_len),
          true
        );
      }
      portEXIT_CRITICAL(&trafficMux);

      if (!permitted) {
        return ERR_OK;
      }
    }
  }

  return originalApLinkOutput
    ? originalApLinkOutput(netif, p)
    : ERR_IF;
}
}

void trafficMonitorBegin() {
  esp_netif_t* apHandle =
    esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");

  if (!apHandle) {
    Serial.println("RangeLink32 traffic monitor: AP netif not found");
    return;
  }

  apNetif = esp_netif_get_netif_impl(apHandle);

  if (!apNetif) {
    Serial.println("RangeLink32 traffic monitor: lwIP netif unavailable");
    return;
  }

  if (!originalApInput) {
    originalApInput = apNetif->input;
    originalApLinkOutput = apNetif->linkoutput;

    apNetif->input = apInputHook;
    apNetif->linkoutput = apLinkOutputHook;

    Serial.println(
      "RangeLink32 traffic monitor: AP hooks enabled"
    );
  }
}

void trafficMonitorLoop() {}

void trafficMonitorSetDefaultAllow(bool allowed) {
  portENTER_CRITICAL(&trafficMux);
  defaultAllow = allowed;
  portEXIT_CRITICAL(&trafficMux);
}

void trafficMonitorConfigureClient(
  const ClientRecord& record,
  bool internetAllowed
) {
  uint8_t mac[6];
  if (!parseMac(record.mac, mac)) return;

  portENTER_CRITICAL(&trafficMux);

  const int index = findOrCreateSlot(mac);
  if (index >= 0) {
    TrafficSlot& slot = slots[index];

    const bool newlyEmpty =
      slot.rxBytes == 0 &&
      slot.txBytes == 0 &&
      slot.dailyRxBytes == 0 &&
      slot.dailyTxBytes == 0;

    if (newlyEmpty) {
      slot.rxBytes = record.rxBytes;
      slot.txBytes = record.txBytes;
      slot.dailyRxBytes = record.dailyRxBytes;
      slot.dailyTxBytes = record.dailyTxBytes;
    }

    slot.internetAllowed = internetAllowed;
    slot.dailyQuotaBytes = record.dailyQuotaBytes;
    slot.bandwidthKbps = record.bandwidthKbps;
  }

  portEXIT_CRITICAL(&trafficMux);
}

bool trafficMonitorGetStats(
  const String& macText,
  uint64_t& rxBytes,
  uint64_t& txBytes,
  uint64_t& dailyRxBytes,
  uint64_t& dailyTxBytes
) {
  uint8_t mac[6];
  if (!parseMac(macText, mac)) return false;

  bool found = false;

  portENTER_CRITICAL(&trafficMux);

  const int index = findSlotByMac(mac);
  if (index >= 0) {
    const TrafficSlot& slot = slots[index];

    rxBytes = slot.rxBytes;
    txBytes = slot.txBytes;
    dailyRxBytes = slot.dailyRxBytes;
    dailyTxBytes = slot.dailyTxBytes;

    found = true;
  }

  portEXIT_CRITICAL(&trafficMux);

  return found;
}

bool trafficMonitorResetUsage(
  const String& macText,
  bool resetTotal
) {
  uint8_t mac[6];
  if (!parseMac(macText, mac)) return false;

  bool found = false;

  portENTER_CRITICAL(&trafficMux);

  const int index = findSlotByMac(mac);
  if (index >= 0) {
    TrafficSlot& slot = slots[index];

    slot.dailyRxBytes = 0;
    slot.dailyTxBytes = 0;

    if (resetTotal) {
      slot.rxBytes = 0;
      slot.txBytes = 0;
    }

    found = true;
  }

  portEXIT_CRITICAL(&trafficMux);

  return found;
}
