#include <Arduino.h>
#include <vector>

extern "C" {
#include "esp_system.h"
#include "mbedtls/gcm.h"
#include "mbedtls/sha256.h"
}

#include "crypto_store.h"

namespace {
constexpr char PREFIX[] = "enc1:";
constexpr uint8_t NONCE_SIZE = 12;
constexpr uint8_t TAG_SIZE = 16;

void deriveKey(uint8_t key[32]) {
  const uint64_t efuse = ESP.getEfuseMac();

  char material[96];
  snprintf(
    material,
    sizeof(material),
    "RangeLink32|%08lX%08lX|credential-store-v1",
    static_cast<unsigned long>(efuse >> 32),
    static_cast<unsigned long>(efuse & 0xFFFFFFFFULL)
  );

  mbedtls_sha256(
    reinterpret_cast<const unsigned char*>(material),
    strlen(material),
    key,
    0
  );
}

String hexEncode(
  const uint8_t* data,
  size_t length
) {
  static const char hex[] = "0123456789abcdef";

  String output;
  output.reserve(length * 2);

  for (size_t i = 0; i < length; ++i) {
    output += hex[(data[i] >> 4) & 0x0F];
    output += hex[data[i] & 0x0F];
  }

  return output;
}

int fromHex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool hexDecode(
  const String& input,
  std::vector<uint8_t>& output
) {
  if (input.length() % 2 != 0) return false;

  output.resize(input.length() / 2);

  for (size_t i = 0; i < output.size(); ++i) {
    const int high = fromHex(input[i * 2]);
    const int low = fromHex(input[i * 2 + 1]);

    if (high < 0 || low < 0) return false;

    output[i] =
      static_cast<uint8_t>((high << 4) | low);
  }

  return true;
}
}

bool isProtectedSecret(
  const String& storedValue
) {
  return storedValue.startsWith(PREFIX);
}

String protectSecret(
  const String& plainText
) {
  if (plainText.length() == 0) return "";

  uint8_t key[32];
  uint8_t nonce[NONCE_SIZE];
  uint8_t tag[TAG_SIZE];

  deriveKey(key);
  esp_fill_random(nonce, sizeof(nonce));

  std::vector<uint8_t> cipher(
    plainText.length()
  );

  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);

  const int setKeyResult =
    mbedtls_gcm_setkey(
      &ctx,
      MBEDTLS_CIPHER_ID_AES,
      key,
      256
    );

  if (setKeyResult != 0) {
    mbedtls_gcm_free(&ctx);
    return "";
  }

  const int result =
    mbedtls_gcm_crypt_and_tag(
      &ctx,
      MBEDTLS_GCM_ENCRYPT,
      plainText.length(),
      nonce,
      sizeof(nonce),
      nullptr,
      0,
      reinterpret_cast<
        const unsigned char*
      >(plainText.c_str()),
      cipher.data(),
      sizeof(tag),
      tag
    );

  mbedtls_gcm_free(&ctx);

  if (result != 0) return "";

  String output = PREFIX;
  output += hexEncode(nonce, sizeof(nonce));
  output += hexEncode(tag, sizeof(tag));
  output += hexEncode(
    cipher.data(),
    cipher.size()
  );

  return output;
}

String unprotectSecret(
  const String& storedValue
) {
  if (!isProtectedSecret(storedValue)) {
    // Legacy migration compatibility.
    return storedValue;
  }

  const String encoded =
    storedValue.substring(strlen(PREFIX));

  std::vector<uint8_t> raw;

  if (!hexDecode(encoded, raw)) return "";

  if (
    raw.size() <
    NONCE_SIZE + TAG_SIZE
  ) {
    return "";
  }

  const uint8_t* nonce = raw.data();
  const uint8_t* tag =
    raw.data() + NONCE_SIZE;

  const uint8_t* cipher =
    raw.data() + NONCE_SIZE + TAG_SIZE;

  const size_t cipherLength =
    raw.size() - NONCE_SIZE - TAG_SIZE;

  uint8_t key[32];
  deriveKey(key);

  std::vector<uint8_t> plain(
    cipherLength + 1,
    0
  );

  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);

  const int setKeyResult =
    mbedtls_gcm_setkey(
      &ctx,
      MBEDTLS_CIPHER_ID_AES,
      key,
      256
    );

  if (setKeyResult != 0) {
    mbedtls_gcm_free(&ctx);
    return "";
  }

  const int result =
    mbedtls_gcm_auth_decrypt(
      &ctx,
      cipherLength,
      nonce,
      NONCE_SIZE,
      nullptr,
      0,
      tag,
      TAG_SIZE,
      cipher,
      plain.data()
    );

  mbedtls_gcm_free(&ctx);

  if (result != 0) return "";

  return String(
    reinterpret_cast<char*>(plain.data())
  );
}
