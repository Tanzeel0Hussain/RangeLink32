# ESP32U Hardware Notes

RangeLink32 targets a classic ESP32 / ESP32U-style module with 2.4 GHz Wi-Fi.

## External antenna

Use a suitable 2.4 GHz antenna connected to the board's RF connector. Follow the board/vendor antenna guidance.

## Placement

The ESP32U must still receive a usable signal from the upstream router. A practical location is usually between the router and the weak-coverage area rather than at the farthest dead zone.

RangeLink32's Smart Placement Assistant uses upstream RSSI as a guide:

- `-60 dBm` or better: excellent
- `-61 to -70 dBm`: good
- `-71 to -80 dBm`: weak
- below `-80 dBm`: move closer to the upstream router

Actual range depends on antenna quality, orientation, walls, interference, channel congestion, regulatory transmit limits and power-supply quality.
