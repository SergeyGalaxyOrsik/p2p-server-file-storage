#pragma once

#include "core/chunk_processor.h"
#include "core/metadata_client.h"
#include "core/node_client.h"
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class DownloadManager {
private:
  MetadataClient *metadataClient;
  NodeClient nodeClient;
  ChunkProcessor chunkProcessor;

  // Callback для прогресса
  std::function<void(size_t, size_t)> progressCallback;

  // Параллелизм
  static const size_t MAX_PARALLEL_DOWNLOADS = 4;
  std::mutex progressMutex;
  size_t completedChunks;

public:
  DownloadManager(MetadataClient *metadataClient);

  // Главный метод скачивания
  bool DownloadFile(const std::string &remoteFilename,
                   const std::string &localPath);

  // Скачивание одного чанка
  bool DownloadChunk(const FileMetadata::ChunkInfo &chunkInfo, Chunk &chunk);

  // Попытка скачать с узлов
  bool TryDownloadFromNodes(const std::string &chunkId,
                           const std::vector<std::string> &nodeIds,
                           Chunk &chunk);

  // Настройка прогресса
  void SetProgressCallback(std::function<void(size_t, size_t)> callback);

private:
  // Внутренние методы
  void ReportProgress(size_t current, size_t total);
  StorageNodeInfo GetNodeInfo(const std::string &nodeId,
                             const FileMetadata &metadata);
};


