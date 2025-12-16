#include "hash_utils.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
#else
#include <openssl/sha.h>
#include <openssl/evp.h>
#endif

namespace HashUtils {

#ifdef _WIN32
// Реализация для Windows через CryptoAPI
std::string CalculateSHA256(const std::vector<uint8_t> &data) {
  HCRYPTPROV hProv = 0;
  HCRYPTHASH hHash = 0;
  DWORD dwHashLen = 0;
  DWORD dwDataLen = static_cast<DWORD>(data.size());
  std::string result;

  // Получение контекста криптопровайдера
  if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES,
                           CRYPT_VERIFYCONTEXT)) {
    return "";
  }

  // Создание хеш-объекта
  if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
    CryptReleaseContext(hProv, 0);
    return "";
  }

  // Добавление данных в хеш
  if (!CryptHashData(hHash, data.data(), dwDataLen, 0)) {
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    return "";
  }

  // Получение размера хеша
  DWORD dwHashSize = 0;
  dwHashLen = sizeof(DWORD);
  if (!CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE *)&dwHashSize, &dwHashLen,
                         0)) {
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    return "";
  }

  // Получение хеша (SHA-256 всегда 32 байта)
  std::vector<BYTE> hash(dwHashSize);
  dwHashLen = dwHashSize;
  if (!CryptGetHashParam(hHash, HP_HASHVAL, hash.data(), &dwHashLen, 0)) {
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    return "";
  }

  // Преобразование в hex строку
  std::stringstream ss;
  for (DWORD i = 0; i < dwHashLen; ++i) {
    ss << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<int>(hash[i]);
  }
  result = ss.str();

  // Очистка
  CryptDestroyHash(hHash);
  CryptReleaseContext(hProv, 0);

  return result;
}

#else
// Реализация для Linux через OpenSSL
std::string CalculateSHA256(const std::vector<uint8_t> &data) {
  EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
  const EVP_MD *md = EVP_sha256();
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hashLen = 0;

  if (mdctx == nullptr) {
    return "";
  }

  if (EVP_DigestInit_ex(mdctx, md, NULL) != 1) {
    EVP_MD_CTX_free(mdctx);
    return "";
  }

  if (EVP_DigestUpdate(mdctx, data.data(), data.size()) != 1) {
    EVP_MD_CTX_free(mdctx);
    return "";
  }

  if (EVP_DigestFinal_ex(mdctx, hash, &hashLen) != 1) {
    EVP_MD_CTX_free(mdctx);
    return "";
  }

  EVP_MD_CTX_free(mdctx);

  // Преобразование в hex строку
  std::stringstream ss;
  for (unsigned int i = 0; i < hashLen; ++i) {
    ss << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<int>(hash[i]);
  }

  return ss.str();
}
#endif

// Вычисление SHA-256 для файла
std::string CalculateSHA256(const std::string &filepath) {
  std::ifstream file(filepath, std::ios::binary);
  if (!file.is_open()) {
    return "";
  }

  // Чтение файла в буфер
  std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
  file.close();

  if (buffer.empty()) {
    return "";
  }

  return CalculateSHA256(buffer);
}

// Проверка хеша
bool VerifyHash(const std::vector<uint8_t> &data,
                const std::string &expectedHash) {
  std::string calculatedHash = CalculateSHA256(data);

  // Приведение к нижнему регистру для сравнения
  std::string lowerExpected = expectedHash;
  std::string lowerCalculated = calculatedHash;

  std::transform(lowerExpected.begin(), lowerExpected.end(),
                 lowerExpected.begin(), ::tolower);
  std::transform(lowerCalculated.begin(), lowerCalculated.end(),
                 lowerCalculated.begin(), ::tolower);

  return lowerCalculated == lowerExpected;
}

} // namespace HashUtils

