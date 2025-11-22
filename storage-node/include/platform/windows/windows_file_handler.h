#pragma once

#include "platform/interfaces/ifile_handler.h"
#include <string>
#include <vector>

class WindowsFileHandler : public IFileHandler {
public:
  WindowsFileHandler() = default;
  ~WindowsFileHandler() override = default;

  bool FileExists(const std::string &filepath) override;
  bool ReadFile(const std::string &filepath,
                std::vector<uint8_t> &buffer) override;
  bool WriteFile(const std::string &filepath,
                 const std::vector<uint8_t> &buffer) override;
  bool DeleteFile(const std::string &filepath) override;
  bool GetFileSize(const std::string &filepath, uint64_t &size) override;
  bool GetFileInfo(const std::string &filepath, FileInfo &info) override;
  bool CreateDirectory(const std::string &path) override;
  bool DirectoryExists(const std::string &path) override;
};
