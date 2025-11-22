#include "platform/windows/windows_file_handler.h"

#define WIN32_LEAN_AND_MEAN
#include <iostream>
#include <windows.h>

bool WindowsFileHandler::FileExists(const std::string &filepath) {
  DWORD dwAttrib = GetFileAttributesA(filepath.c_str());
  return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
          !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool WindowsFileHandler::ReadFile(const std::string &filepath,
                                  std::vector<uint8_t> &buffer) {
  HANDLE hFile = CreateFileA(filepath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                             NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  if (hFile == INVALID_HANDLE_VALUE) {
    return false;
  }

  LARGE_INTEGER fileSize;
  if (!GetFileSizeEx(hFile, &fileSize)) {
    CloseHandle(hFile);
    return false;
  }

  buffer.resize(static_cast<size_t>(fileSize.QuadPart));

  if (fileSize.QuadPart == 0) {
    CloseHandle(hFile);
    return true;
  }

  DWORD bytesRead;
  // Note: This simple implementation reads the whole file at once.
  // For very large files > 4GB, we would need a loop and multiple ReadFile
  // calls if we were reading into a buffer larger than DWORD max, but vector
  // size is size_t. ReadFile takes DWORD for number of bytes to read. If size_t
  // is 64-bit, we might have an issue if file > 4GB. However, for this
  // coursework, files are split into 1MB chunks, so this is fine.

  bool success =
      ::ReadFile(hFile, buffer.data(), static_cast<DWORD>(buffer.size()),
                 &bytesRead, NULL);

  CloseHandle(hFile);
  return success && (bytesRead == buffer.size());
}

bool WindowsFileHandler::WriteFile(const std::string &filepath,
                                   const std::vector<uint8_t> &buffer) {
  HANDLE hFile = CreateFileA(filepath.c_str(), GENERIC_WRITE, 0, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

  if (hFile == INVALID_HANDLE_VALUE) {
    return false;
  }

  DWORD bytesWritten;
  bool success =
      ::WriteFile(hFile, buffer.data(), static_cast<DWORD>(buffer.size()),
                  &bytesWritten, NULL);

  CloseHandle(hFile);
  return success && (bytesWritten == buffer.size());
}

bool WindowsFileHandler::DeleteFile(const std::string &filepath) {
  return ::DeleteFileA(filepath.c_str()) != 0;
}

bool WindowsFileHandler::GetFileSize(const std::string &filepath,
                                     uint64_t &size) {
  HANDLE hFile = CreateFileA(filepath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                             NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  if (hFile == INVALID_HANDLE_VALUE) {
    return false;
  }

  LARGE_INTEGER fileSize;
  bool success = GetFileSizeEx(hFile, &fileSize);
  CloseHandle(hFile);

  if (success) {
    size = static_cast<uint64_t>(fileSize.QuadPart);
    return true;
  }
  return false;
}

bool WindowsFileHandler::GetFileInfo(const std::string &filepath,
                                     FileInfo &info) {
  WIN32_FILE_ATTRIBUTE_DATA fileInfo;
  if (!GetFileAttributesExA(filepath.c_str(), GetFileExInfoStandard,
                            &fileInfo)) {
    info.exists = false;
    return false;
  }

  info.exists = true;
  ULARGE_INTEGER fileSize;
  fileSize.LowPart = fileInfo.nFileSizeLow;
  fileSize.HighPart = fileInfo.nFileSizeHigh;
  info.size = fileSize.QuadPart;

  // Convert FILETIME to std::chrono::system_clock::time_point
  ULARGE_INTEGER ft;
  ft.LowPart = fileInfo.ftLastWriteTime.dwLowDateTime;
  ft.HighPart = fileInfo.ftLastWriteTime.dwHighDateTime;

  // Windows file time is 100-nanosecond intervals since January 1, 1601 (UTC).
  // UNIX epoch is January 1, 1970.
  // Difference is 116444736000000000 intervals.
  const uint64_t EPOCH_DIFFERENCE = 116444736000000000ULL;

  if (ft.QuadPart >= EPOCH_DIFFERENCE) {
    uint64_t unixTime =
        (ft.QuadPart - EPOCH_DIFFERENCE) / 10000; // Convert to milliseconds
    info.lastModified =
        std::chrono::system_clock::from_time_t(unixTime / 1000) +
        std::chrono::milliseconds(unixTime % 1000);
  } else {
    info.lastModified = std::chrono::system_clock::now(); // Fallback
  }

  return true;
}

bool WindowsFileHandler::CreateDirectory(const std::string &path) {
  if (DirectoryExists(path)) {
    return true;
  }
  return CreateDirectoryA(path.c_str(), NULL) != 0;
}

bool WindowsFileHandler::DirectoryExists(const std::string &path) {
  DWORD dwAttrib = GetFileAttributesA(path.c_str());
  return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
          (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}
