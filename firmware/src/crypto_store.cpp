#include <Arduino.h>
#include <Preferences.h>
#include <vector>

extern "C" {
#include "esp_system.h"
#include "mbedtls/gcm.h"
#include "mbedtls/sha256.h"
}

#include "crypto_store.h"

namespace {
constexpr char PREFIX_V1[] = "enc1:";
constexpr char PREFIX_V2[] = "enc2:";
constexpr uint8_t NONCE_SIZE = 12;
constexpr uint8_t TAG_SIZE = 16;
constexpr size_t MASTER_KEY_SIZE = 32;

void deriveLegacyKey(uint8_t key[32]) {
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

bool loadOrCreateMasterSecret(uint8_t out[MASTER_KEY_SIZE]) {
  Preferences securePrefs;
  if (!securePrefs.begin("rl32-sec", false)) {
    return false;
  }

  const size_t storedLength =
    securePrefs.getBytesLength("master");

  if (storedLength == MASTER_KEY_SIZE) {
    const size_t read =
      securePrefs.getBytes(
        "master",
        out,
        MASTER_KEY_SIZE
      );
    securePrefs.end();
    return read == MASTER_KEY_SIZE;
  }

  esp_fill_random(out, MASTER_KEY_SIZE);

  const size_t written =
    securePrefs.putBytes(
      "master",
      out,
      MASTER_KEY_SIZE
    );

  securePrefs.end();
  return written == MASTER_KEY_SIZE;
}

bool deriveCurrentKey(uint8_t key[32]) {
  uint8_t master[MASTER_KEY_SIZE];
  if (!loadOrCreateMasterSecret(master)) {
    return false;
  }

  const uint64_t efuse = ESP.getEfuseMac();

  uint8_t material[MASTER_KEY_SIZE + sizeof(efuse) + 24];
  memset(material, 0, sizeof(material));

  memcpy(material, master, MASTER_KEY_SIZE);
  memcpy(material + MASTER_KEY_SIZE, &efuse, sizeof(efuse));

  const char context[] = "RangeLink32-credential-v2";
  memcpy(
    material + MASTER_KEY_SIZE + sizeof(efuse),
    context,
    sizeof(context) - 1
  );

  mbedtls_sha256(
    material,
    sizeof(material),
    key,
    0
  );

  memset(master, 0, sizeof(master));
  memset(material, 0, sizeof(material));
  return true;
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

String decryptWithKey(
  const String& encoded,
  const uint8_t key[32]
) {
  std::vector<uint8_t> raw;
  if (!hexDecode(encoded, raw)) return "";

  if (raw.size() < NONCE_SIZE + TAG_SIZE) {
    return "";
  }

  const uint8_t* nonce = raw.data();
  const uint8_t* tag =
    raw.data() + NONCE_SIZE;
  const uint8_t* cipher =
    raw.data() + NONCE_SIZE + TAG_SIZE;

  const size_t cipherLength =
    raw.size() - NONCE_SIZE - TAG_SIZE;

  std::vector<uint8_t> plain(
    cipherLength + 1,
    0
  );

  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);

  if (
    mbedtls_gcm_setkey(
      &ctx,
      MBEDTLS_CIPHER_ID_AES,
      key,
      256
    ) != 0
  ) {
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
}

bool isProtectedSecret(
  const String& storedValue
) {
  return
    storedValue.startsWith(PREFIX_V1) ||
    storedValue.startsWith(PREFIX_V2);
}

String protectSecret(
  const String& plainText
) {
  if (plainText.length() == 0) return "";

  uint8_t key[32];
  if (!deriveCurrentKey(key)) return "";

  uint8_t nonce[NONCE_SIZE];
  uint8_t tag[TAG_SIZE];

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
    memset(key, 0, sizeof(key));
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
  memset(key, 0, sizeof(key));

  if (result != 0) return "";

  String output = PREFIX_V2;
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
    // Legacy plaintext migration compatibility.
    return storedValue;
  }

  uint8_t key[32];
  String encoded;

  if (storedValue.startsWith(PREFIX_V2)) {
    if (!deriveCurrentKey(key)) return "";
    encoded =
      storedValue.substring(strlen(PREFIX_V2));
  } else {
    deriveLegacyKey(key);
    encoded =
      storedValue.substring(strlen(PREFIX_V1));
  }

  const String plain =
    decryptWithKey(encoded, key);

  memset(key, 0, sizeof(key));
  return plain;
}
