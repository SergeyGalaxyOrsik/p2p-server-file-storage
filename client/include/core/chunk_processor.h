#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Forward declaration
namespace HashUtils {
  std::string CalculateSHA256(const std::vector<uint8_t> &data);
}

struct Chunk {
  std::string chunkId; // SHA-256 хеш чанка
  size_t index; // Порядковый номер (0-based)
  size_t size; // Размер чанка в байтах
  std::vector<uint8_t> data; // Данные чанка

  // Валидация
  bool IsValid() const;
};

class ChunkProcessor {
private:
  static const size_t CHUNK_SIZE = 1048576; // 1 МБ

public:
  // Разбиение файла на чанки
  std::vector<Chunk> SplitFile(const std::string &filepath);

  // Сборка файла из чанков
  bool AssembleFile(const std::vector<Chunk> &chunks,
                   const std::string &outputPath);

  // Валидация чанка
  bool ValidateChunk(const Chunk &chunk);

private:
  // Внутренние методы
  bool ValidateChunkSequence(const std::vector<Chunk> &chunks);
};

