#pragma once

#include <string>

class IPathHandler {
public:
  virtual ~IPathHandler() = default;

  // Операции с путями
  virtual std::string Join(const std::string &path1,
                           const std::string &path2) = 0;
  virtual std::string GetDirectory(const std::string &filepath) = 0;
  virtual std::string GetFilename(const std::string &filepath) = 0;
  virtual std::string GetExtension(const std::string &filepath) = 0;

  // Нормализация
  virtual std::string Normalize(const std::string &path) = 0;
  virtual std::string Absolute(const std::string &path) = 0;

  // Разделители
  virtual char GetSeparator() const = 0;
  virtual std::string GetSeparatorString() const = 0;
};