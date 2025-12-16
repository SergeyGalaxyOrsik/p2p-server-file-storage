#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct ChunkInfo {
  std::string chunkId; // SHA-256 хеш (64 символа hex)
  size_t index; // Порядковый номер (0-based)
  size_t size; // Размер чанка в байтах
  std::vector<std::string> nodeIds; // Узлы, хранящие чанк (репликация)

  // Валидация
  bool IsValid() const;
};

struct FileMetadata {
  std::string filename; // Имя файла в системе
  uint64_t totalSize; // Общий размер файла
  std::vector<ChunkInfo> chunks; // Список чанков
  std::chrono::time_point<std::chrono::steady_clock> uploadTime;
  std::chrono::time_point<std::chrono::steady_clock> lastAccessed;

  // Валидация
  bool IsValid() const;

  // Утилиты
  size_t GetChunkCount() const { return chunks.size(); }
  bool HasChunk(const std::string &chunkId) const;
};

class MetadataManager {
private:
  std::unordered_map<std::string, FileMetadata> files;
  mutable std::mutex filesMutex;

  // Статистика
  size_t totalFiles;
  uint64_t totalBytes;

public:
  MetadataManager();
  ~MetadataManager();

  // Управление файлами
  bool RegisterFile(const std::string &filename, uint64_t size,
                    const std::vector<ChunkInfo> &chunks);
  bool DeleteFile(const std::string &filename);
  FileMetadata *GetFileMetadata(const std::string &filename);

  // Поиск и список
  std::vector<std::string> ListFiles();
  std::vector<FileMetadata> GetAllFiles();
  bool FileExists(const std::string &filename);

  // Работа с чанками
  std::vector<ChunkInfo> GetFileChunks(const std::string &filename);
  ChunkInfo *GetChunkInfo(const std::string &filename,
                         const std::string &chunkId);

  // Статистика
  size_t GetFileCount() const;
  uint64_t GetTotalBytes() const;

private:
  // Внутренние методы
  bool ValidateFileMetadata(const FileMetadata &metadata);
  void UpdateStatistics();
  std::string SanitizeFilename(const std::string &filename);
  bool ValidateChunkSequence(const std::vector<ChunkInfo> &chunks);
};


