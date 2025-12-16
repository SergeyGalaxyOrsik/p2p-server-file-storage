#include "metadata_manager.h"

#include <algorithm>
#include <cctype>
#include <sstream>

// Валидация ChunkInfo
bool ChunkInfo::IsValid() const {
  return !chunkId.empty() && chunkId.length() == 64 && !nodeIds.empty() &&
         size > 0;
}

// Валидация FileMetadata
bool FileMetadata::IsValid() const {
  return !filename.empty() && totalSize > 0 && !chunks.empty();
}

// Проверка наличия чанка
bool FileMetadata::HasChunk(const std::string &chunkId) const {
  for (const auto &chunk : chunks) {
    if (chunk.chunkId == chunkId) {
      return true;
    }
  }
  return false;
}

// Конструктор
MetadataManager::MetadataManager() : totalFiles(0), totalBytes(0) {}

// Деструктор
MetadataManager::~MetadataManager() = default;

// Санитизация имени файла
std::string MetadataManager::SanitizeFilename(const std::string &filename) {
  std::string sanitized = filename;

  // Удаление опасных символов
  sanitized.erase(
      std::remove_if(sanitized.begin(), sanitized.end(),
                    [](char c) {
                      return c == '/' || c == '\\' || c == ':' || c == '*' ||
                             c == '?' || c == '"' || c == '<' || c == '>' ||
                             c == '|';
                    }),
      sanitized.end());

  // Удаление пробелов в начале и конце
  sanitized.erase(0, sanitized.find_first_not_of(" \t"));
  sanitized.erase(sanitized.find_last_not_of(" \t") + 1);

  return sanitized;
}

// Валидация последовательности чанков
bool MetadataManager::ValidateChunkSequence(
    const std::vector<ChunkInfo> &chunks) {
  if (chunks.empty()) {
    return false;
  }

  // Создаём копию для сортировки
  std::vector<ChunkInfo> sortedChunks = chunks;
  std::sort(sortedChunks.begin(), sortedChunks.end(),
            [](const ChunkInfo &a, const ChunkInfo &b) {
              return a.index < b.index;
            });

  // Проверяем последовательность индексов
  for (size_t i = 0; i < sortedChunks.size(); ++i) {
    if (sortedChunks[i].index != i) {
      return false; // Пропущен индекс
    }
  }

  return true;
}

// Валидация метаданных файла
bool MetadataManager::ValidateFileMetadata(const FileMetadata &metadata) {
  // Проверка базовой валидности
  if (!metadata.IsValid()) {
    return false;
  }

  // Проверка валидности всех чанков
  for (const auto &chunk : metadata.chunks) {
    if (!chunk.IsValid()) {
      return false;
    }
  }

  // Проверка последовательности индексов
  if (!ValidateChunkSequence(metadata.chunks)) {
    return false;
  }

  // Проверка соответствия общего размера
  uint64_t calculatedSize = 0;
  for (const auto &chunk : metadata.chunks) {
    calculatedSize += chunk.size;
  }

  if (calculatedSize != metadata.totalSize) {
    return false; // Размеры не совпадают
  }

  return true;
}

// Регистрация файла
bool MetadataManager::RegisterFile(const std::string &filename, uint64_t size,
                                   const std::vector<ChunkInfo> &chunks) {
  // Санитизация имени файла
  std::string sanitizedFilename = SanitizeFilename(filename);
  if (sanitizedFilename.empty()) {
    return false;
  }

  // Создание копии чанков для сортировки
  std::vector<ChunkInfo> sortedChunks = chunks;
  std::sort(sortedChunks.begin(), sortedChunks.end(),
            [](const ChunkInfo &a, const ChunkInfo &b) {
              return a.index < b.index;
            });

  // Создание метаданных
  FileMetadata metadata;
  metadata.filename = sanitizedFilename;
  metadata.totalSize = size;
  metadata.chunks = sortedChunks;
  metadata.uploadTime = std::chrono::steady_clock::now();
  metadata.lastAccessed = std::chrono::steady_clock::now();

  // Валидация метаданных
  if (!ValidateFileMetadata(metadata)) {
    return false;
  }

  // Сохранение в map с мьютексом
  {
    std::lock_guard<std::mutex> lock(filesMutex);
    files[sanitizedFilename] = metadata;
  }

  // Обновление статистики
  UpdateStatistics();

  return true;
}

// Удаление файла
bool MetadataManager::DeleteFile(const std::string &filename) {
  std::string sanitizedFilename = SanitizeFilename(filename);

  std::lock_guard<std::mutex> lock(filesMutex);
  auto it = files.find(sanitizedFilename);
  if (it != files.end()) {
    files.erase(it);
    UpdateStatistics();
    return true;
  }

  return false;
}

// Получение метаданных файла
FileMetadata *MetadataManager::GetFileMetadata(const std::string &filename) {
  std::string sanitizedFilename = SanitizeFilename(filename);

  std::lock_guard<std::mutex> lock(filesMutex);
  auto it = files.find(sanitizedFilename);
  if (it != files.end()) {
    // Обновление времени последнего доступа
    it->second.lastAccessed = std::chrono::steady_clock::now();
    return &(it->second);
  }

  return nullptr;
}

// Проверка существования файла
bool MetadataManager::FileExists(const std::string &filename) {
  std::string sanitizedFilename = SanitizeFilename(filename);

  std::lock_guard<std::mutex> lock(filesMutex);
  return files.find(sanitizedFilename) != files.end();
}

// Список имён файлов
std::vector<std::string> MetadataManager::ListFiles() {
  std::vector<std::string> fileList;

  std::lock_guard<std::mutex> lock(filesMutex);
  fileList.reserve(files.size());

  for (const auto &pair : files) {
    fileList.push_back(pair.first);
  }

  return fileList;
}

// Получение всех метаданных
std::vector<FileMetadata> MetadataManager::GetAllFiles() {
  std::vector<FileMetadata> allFiles;

  std::lock_guard<std::mutex> lock(filesMutex);
  allFiles.reserve(files.size());

  for (const auto &pair : files) {
    allFiles.push_back(pair.second);
  }

  return allFiles;
}

// Получение чанков файла
std::vector<ChunkInfo> MetadataManager::GetFileChunks(const std::string &filename) {
  std::string sanitizedFilename = SanitizeFilename(filename);

  std::lock_guard<std::mutex> lock(filesMutex);
  auto it = files.find(sanitizedFilename);
  if (it != files.end()) {
    return it->second.chunks;
  }

  return std::vector<ChunkInfo>();
}

// Получение информации о чанке
ChunkInfo *MetadataManager::GetChunkInfo(const std::string &filename,
                                        const std::string &chunkId) {
  std::string sanitizedFilename = SanitizeFilename(filename);

  std::lock_guard<std::mutex> lock(filesMutex);
  auto it = files.find(sanitizedFilename);
  if (it != files.end()) {
    for (auto &chunk : it->second.chunks) {
      if (chunk.chunkId == chunkId) {
        return &chunk;
      }
    }
  }

  return nullptr;
}

// Обновление статистики
void MetadataManager::UpdateStatistics() {
  size_t count = 0;
  uint64_t bytes = 0;

  {
    std::lock_guard<std::mutex> lock(filesMutex);
    for (const auto &pair : files) {
      count++;
      bytes += pair.second.totalSize;
    }
  }

  totalFiles = count;
  totalBytes = bytes;
}

// Количество файлов
size_t MetadataManager::GetFileCount() const {
  std::lock_guard<std::mutex> lock(filesMutex);
  return files.size();
}

// Общий объём данных
uint64_t MetadataManager::GetTotalBytes() const {
  uint64_t total = 0;

  std::lock_guard<std::mutex> lock(filesMutex);
  for (const auto &pair : files) {
    total += pair.second.totalSize;
  }

  return total;
}


