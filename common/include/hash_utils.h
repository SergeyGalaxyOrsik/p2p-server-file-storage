#pragma once

#include <string>
#include <vector>

namespace HashUtils {
  // Вычисление SHA-256 для данных
  std::string CalculateSHA256(const std::vector<uint8_t> &data);

  // Вычисление SHA-256 для файла
  std::string CalculateSHA256(const std::string &filepath);

  // Проверка хеша
  bool VerifyHash(const std::vector<uint8_t> &data,
                  const std::string &expectedHash);
}


