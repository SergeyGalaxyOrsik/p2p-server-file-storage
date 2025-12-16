#include "core/chunk_processor.h"

#include "hash_utils.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>

// Валидация чанка
bool Chunk::IsValid() const {
  return !chunkId.empty() && chunkId.length() == 64 && size > 0 &&
         data.size() == size;
}

// Разбиение файла на чанки
std::vector<Chunk> ChunkProcessor::SplitFile(const std::string &filepath) {
  std::vector<Chunk> chunks;

  std::ifstream file(filepath, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Error: Failed to open file: " << filepath << std::endl;
    return chunks;
  }

  size_t index = 0;
  std::vector<uint8_t> buffer(CHUNK_SIZE);

  while (true) {
    // Чтение чанка
    file.read(reinterpret_cast<char *>(buffer.data()), CHUNK_SIZE);
    std::streamsize bytesRead = file.gcount();

    if (bytesRead == 0) {
      break; // Конец файла
    }

    // Создание чанка
    Chunk chunk;
    chunk.index = index;
    chunk.size = static_cast<size_t>(bytesRead);
    chunk.data.resize(chunk.size);
    std::copy(buffer.begin(), buffer.begin() + chunk.size, chunk.data.begin());

    // Вычисление хеша
    chunk.chunkId = HashUtils::CalculateSHA256(chunk.data);
    if (chunk.chunkId.empty()) {
      std::cerr << "Error: Failed to calculate hash for chunk " << index
                << std::endl;
      file.close();
      return std::vector<Chunk>(); // Ошибка
    }

    chunks.push_back(chunk);
    index++;

    // Проверка на конец файла
    if (bytesRead < static_cast<std::streamsize>(CHUNK_SIZE)) {
      break;
    }
  }

  file.close();
  return chunks;
}

// Валидация чанка
bool ChunkProcessor::ValidateChunk(const Chunk &chunk) {
  // Проверка структуры
  if (!chunk.IsValid()) {
    return false;
  }

  // Проверка хеша
  std::string calculatedHash = HashUtils::CalculateSHA256(chunk.data);
  if (calculatedHash.empty()) {
    return false;
  }

  // Приведение к нижнему регистру для сравнения
  std::string lowerExpected = chunk.chunkId;
  std::string lowerCalculated = calculatedHash;

  std::transform(lowerExpected.begin(), lowerExpected.end(),
                 lowerExpected.begin(), ::tolower);
  std::transform(lowerCalculated.begin(), lowerCalculated.end(),
                 lowerCalculated.begin(), ::tolower);

  return lowerCalculated == lowerExpected;
}

// Валидация последовательности чанков
bool ChunkProcessor::ValidateChunkSequence(
    const std::vector<Chunk> &chunks) {
  if (chunks.empty()) {
    return false;
  }

  // Создаём копию для сортировки
  std::vector<Chunk> sortedChunks = chunks;
  std::sort(sortedChunks.begin(), sortedChunks.end(),
            [](const Chunk &a, const Chunk &b) { return a.index < b.index; });

  // Проверяем последовательность индексов
  for (size_t i = 0; i < sortedChunks.size(); ++i) {
    if (sortedChunks[i].index != i) {
      return false; // Пропущен индекс
    }
  }

  return true;
}

// Сборка файла из чанков
bool ChunkProcessor::AssembleFile(const std::vector<Chunk> &chunks,
                                  const std::string &outputPath) {
  if (chunks.empty()) {
    std::cerr << "Error: No chunks to assemble" << std::endl;
    return false;
  }

  // Проверка последовательности
  if (!ValidateChunkSequence(chunks)) {
    std::cerr << "Error: Invalid chunk sequence" << std::endl;
    return false;
  }

  // Сортировка чанков по индексу
  std::vector<Chunk> sortedChunks = chunks;
  std::sort(sortedChunks.begin(), sortedChunks.end(),
            [](const Chunk &a, const Chunk &b) { return a.index < b.index; });

  // Валидация каждого чанка перед записью
  for (const auto &chunk : sortedChunks) {
    if (!ValidateChunk(chunk)) {
      std::cerr << "Error: Invalid chunk at index " << chunk.index
                << std::endl;
      return false;
    }
  }

  // Запись файла
  std::ofstream file(outputPath, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Error: Failed to create output file: " << outputPath
              << std::endl;
    return false;
  }

  // Запись чанков
  for (const auto &chunk : sortedChunks) {
    file.write(reinterpret_cast<const char *>(chunk.data.data()),
               static_cast<std::streamsize>(chunk.data.size()));

    if (!file.good()) {
      std::cerr << "Error: Failed to write chunk at index " << chunk.index
                << std::endl;
      file.close();
      return false;
    }
  }

  file.close();
  return true;
}

