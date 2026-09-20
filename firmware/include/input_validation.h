#pragma once
#include <stddef.h>

namespace RangeLinkValidation {
inline bool parseNonNegativeDecimal(
  const char* text,
  double maximum,
  double& value
) {
  if (!text || !*text || maximum < 0.0) {
    return false;
  }

  bool seenDigit = false;
  bool seenDot = false;
  size_t length = 0;
  double result = 0.0;
  double fraction = 0.1;

  for (const char* p = text; *p; ++p) {
    if (++length > 32) return false;

    const char c = *p;

    if (c >= '0' && c <= '9') {
      seenDigit = true;
      const double digit =
        static_cast<double>(c - '0');

      if (!seenDot) {
        if (
          result >
          (maximum - digit) / 10.0
        ) {
          return false;
        }

        result =
          result * 10.0 + digit;
      } else {
        result += digit * fraction;
        fraction *= 0.1;

        if (result > maximum) {
          return false;
        }
      }

      continue;
    }

    if (c == '.' && !seenDot) {
      seenDot = true;
      continue;
    }

    return false;
  }

  if (!seenDigit || result > maximum) {
    return false;
  }

  value = result;
  return true;
}
}
