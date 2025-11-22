#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class IFileHandler {
public:
  struct FileInfo {
    uint64_t size;
    std::chrono::time_point<std::chrono::system_clock> lastModified;
    bool exists;
  };

  virtual ~IFileHandler() = default;

  // Проверка существования файла
  virtual bool FileExists(const std::string &filepath) = 0;

  // Чтение файла
  virtual bool ReadFile(const std::string &filepath,
                        std::vector<uint8_t> &buffer) = 0;

  // Запись файла
  virtual bool WriteFile(const std::string &filepath,
                         const std::vector<uint8_t> &buffer) = 0;

  // Удаление файла
  virtual bool DeleteFile(const std::string &filepath) = 0;

  // Получение размера файла
  virtual bool GetFileSize(const std::string &filepath, uint64_t &size) = 0;

  // Получение информации о файле
  virtual bool GetFileInfo(const std::string &filepath, FileInfo &info) = 0;

  // Утилиты
  virtual bool CreateDirectory(const std::string &path) = 0;
  virtual bool DirectoryExists(const std::string &path) = 0;
};