#pragma once

#include <cstdint>
#include <string>

class IDiskHandler {
public:
  virtual ~IDiskHandler() = default;

  // Информация о диске
  virtual uint64_t GetFreeSpace(const std::string &path) = 0;
  virtual uint64_t GetTotalSpace(const std::string &path) = 0;
  virtual uint64_t GetUsedSpace(const std::string &path) = 0;

  // Утилиты
  virtual bool IsPathValid(const std::string &path) = 0;
};