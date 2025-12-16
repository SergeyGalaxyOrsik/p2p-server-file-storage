#include "core/download_manager.h"

#include "core/metadata_client.h"
#include "hash_utils.h"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <thread>

DownloadManager::DownloadManager(MetadataClient *metadataClient)
    : metadataClient(metadataClient), completedChunks(0) {}

// Настройка прогресса
void DownloadManager::SetProgressCallback(
    std::function<void(size_t, size_t)> callback) {
  progressCallback = callback;
}

// Отчёт о прогрессе
void DownloadManager::ReportProgress(size_t current, size_t total) {
  std::lock_guard<std::mutex> lock(progressMutex);
  if (progressCallback) {
    progressCallback(current, total);
  }
}

// Получение информации об узле из метаданных
StorageNodeInfo DownloadManager::GetNodeInfo(const std::string &nodeId,
                                             const FileMetadata &metadata) {
  // Ищем узел в метаданных (упрощённо - нужно будет получать из NodeManager)
  // Пока возвращаем пустую структуру
  StorageNodeInfo node;
  node.nodeId = nodeId;
  // IP и порт нужно получать из другого источника
  return node;
}

// Попытка скачать с узлов
bool DownloadManager::TryDownloadFromNodes(
    const std::string &chunkId, const std::vector<std::string> &nodeIds,
    Chunk &chunk) {
  // Перебор узлов по списку
  for (const std::string &nodeId : nodeIds) {
    // Получение информации об узле из кэша MetadataClient
    StorageNodeInfo node;
    if (!metadataClient->GetNodeInfo(nodeId, node)) {
      // Узел не найден в кэше, пропускаем
      continue;
    }

    // Попытка скачать с узла
    std::vector<uint8_t> data;
    if (nodeClient.GetChunk(node, chunkId, data)) {
      // Валидация скачанного чанка
      std::string calculatedHash = HashUtils::CalculateSHA256(data);
      std::string lowerCalculated = calculatedHash;
      std::string lowerExpected = chunkId;
      std::transform(lowerCalculated.begin(), lowerCalculated.end(),
                     lowerCalculated.begin(), ::tolower);
      std::transform(lowerExpected.begin(), lowerExpected.end(),
                     lowerExpected.begin(), ::tolower);

      if (lowerCalculated == lowerExpected) {
        chunk.data = data;
        chunk.size = data.size();
        chunk.chunkId = chunkId;
        return true; // Успешно скачали
      }
    }
  }

  return false; // Не удалось скачать ни с одного узла
}

// Скачивание одного чанка
bool DownloadManager::DownloadChunk(const FileMetadata::ChunkInfo &chunkInfo,
                                    Chunk &chunk) {
  // Используем TryDownloadFromNodes для скачивания
  return TryDownloadFromNodes(chunkInfo.chunkId, chunkInfo.nodeIds, chunk);
}

// Главный метод скачивания
bool DownloadManager::DownloadFile(const std::string &remoteFilename,
                                  const std::string &localPath) {
  // Запрос метаданных файла
  std::cout << "Requesting file metadata..." << std::endl;
  FileMetadata metadata = metadataClient->RequestDownload(remoteFilename);

  if (metadata.filename.empty() || metadata.chunks.empty()) {
    std::cerr << "Error: File not found or invalid metadata" << std::endl;
    return false;
  }

  std::cout << "File metadata received: " << metadata.chunks.size()
            << " chunks, " << metadata.totalSize << " bytes" << std::endl;

  // Скачивание чанков
  std::cout << "Downloading chunks..." << std::endl;
  std::vector<Chunk> chunks;
  completedChunks = 0;

  // Последовательное скачивание (можно сделать параллельным позже)
  for (size_t i = 0; i < metadata.chunks.size(); ++i) {
    const auto &chunkInfo = metadata.chunks[i];
    Chunk chunk;
    chunk.index = chunkInfo.index;
    chunk.chunkId = chunkInfo.chunkId;
    chunk.size = chunkInfo.size;

    // Попытка скачать с узлов
    if (!TryDownloadFromNodes(chunkInfo.chunkId, chunkInfo.nodeIds, chunk)) {
      std::cerr << "Error: Failed to download chunk " << i << std::endl;
      return false;
    }

    chunks.push_back(chunk);
    completedChunks++;
    ReportProgress(completedChunks, metadata.chunks.size());
  }

  // Сборка файла
  std::cout << "Assembling file..." << std::endl;
  if (!chunkProcessor.AssembleFile(chunks, localPath)) {
    std::cerr << "Error: Failed to assemble file" << std::endl;
    return false;
  }

  std::cout << "File downloaded successfully!" << std::endl;
  return true;
}

