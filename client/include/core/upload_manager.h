#pragma once

#include "core/chunk_processor.h"
#include "core/metadata_client.h"
#include "core/node_client.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class UploadManager {
private:
  MetadataClient *metadataClient;
  NodeClient nodeClient;
  ChunkProcessor chunkProcessor;

  // Конфигурация
  static const size_t REPLICATION_FACTOR = 2; // 2 копии каждого чанка

  // Callback для прогресса
  std::function<void(size_t, size_t)> progressCallback;

public:
  UploadManager(MetadataClient *metadataClient);

  // Главный метод загрузки
  bool UploadFile(const std::string &localPath,
                 const std::string &remoteFilename);

  // Загрузка чанка с репликацией
  bool UploadChunk(const Chunk &chunk,
                  const std::vector<StorageNodeInfo> &nodes,
                  std::vector<std::string> &uploadedNodeIds);

  // Выбор узлов для чанка
  std::vector<StorageNodeInfo>
  SelectNodesForChunk(const std::vector<StorageNodeInfo> &nodes,
                     size_t chunkIndex);

  // Настройка прогресса
  void SetProgressCallback(std::function<void(size_t, size_t)> callback);

private:
  // Внутренние методы
  void ReportProgress(size_t current, size_t total);
};

