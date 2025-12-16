#include "core/upload_manager.h"

#include <algorithm>
#include <fstream>
#include <iostream>

UploadManager::UploadManager(MetadataClient *metadataClient)
    : metadataClient(metadataClient) {}

// Настройка прогресса
void UploadManager::SetProgressCallback(
    std::function<void(size_t, size_t)> callback) {
  progressCallback = callback;
}

// Отчёт о прогрессе
void UploadManager::ReportProgress(size_t current, size_t total) {
  if (progressCallback) {
    progressCallback(current, total);
  }
}

// Выбор узлов для чанка
std::vector<StorageNodeInfo> UploadManager::SelectNodesForChunk(
    const std::vector<StorageNodeInfo> &nodes, size_t chunkIndex) {
  std::vector<StorageNodeInfo> selectedNodes;

  if (nodes.size() < REPLICATION_FACTOR) {
    return selectedNodes; // Недостаточно узлов
  }

  // Распределение узлов равномерно
  // Используем round-robin для балансировки нагрузки
  for (size_t i = 0; i < REPLICATION_FACTOR; ++i) {
    size_t nodeIndex = (chunkIndex * REPLICATION_FACTOR + i) % nodes.size();
    selectedNodes.push_back(nodes[nodeIndex]);
  }

  return selectedNodes;
}

// Загрузка чанка с репликацией
bool UploadManager::UploadChunk(const Chunk &chunk,
                                const std::vector<StorageNodeInfo> &nodes,
                                std::vector<std::string> &uploadedNodeIds) {
  uploadedNodeIds.clear();

  // Выбор узлов для этого чанка
  std::vector<StorageNodeInfo> selectedNodes =
      SelectNodesForChunk(nodes, chunk.index);

  if (selectedNodes.size() < REPLICATION_FACTOR) {
    std::cerr << "Error: Not enough nodes for chunk " << chunk.index
              << std::endl;
    return false;
  }

  // Загрузка на все узлы (репликация)
  bool allSuccess = true;
  for (const auto &node : selectedNodes) {
    if (nodeClient.StoreChunk(node, chunk.chunkId, chunk.data)) {
      uploadedNodeIds.push_back(node.nodeId);
    } else {
      std::cerr << "Warning: Failed to store chunk " << chunk.index
                << " on node " << node.nodeId << std::endl;
      allSuccess = false;
      // Продолжаем попытки с другими узлами
    }
  }

  // Требуем успешную загрузку хотя бы на REPLICATION_FACTOR узлов
  return uploadedNodeIds.size() >= REPLICATION_FACTOR;
}

// Главный метод загрузки
bool UploadManager::UploadFile(const std::string &localPath,
                               const std::string &remoteFilename) {
  // Проверка существования файла
  std::ifstream file(localPath, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Error: File not found: " << localPath << std::endl;
    return false;
  }
  file.close();

  // Разбиение файла на чанки
  std::cout << "Splitting file into chunks..." << std::endl;
  std::vector<Chunk> chunks = chunkProcessor.SplitFile(localPath);
  if (chunks.empty()) {
    std::cerr << "Error: Failed to split file into chunks" << std::endl;
    return false;
  }

  std::cout << "File split into " << chunks.size() << " chunks" << std::endl;

  // Вычисление общего размера
  uint64_t totalSize = 0;
  for (const auto &chunk : chunks) {
    totalSize += chunk.size;
  }

  // Запрос узлов у Metadata Server
  std::cout << "Requesting nodes from metadata server..." << std::endl;
  std::vector<StorageNodeInfo> nodes =
      metadataClient->RequestUploadNodes(remoteFilename, totalSize);

  if (nodes.size() < REPLICATION_FACTOR) {
    std::cerr << "Error: Not enough storage nodes available" << std::endl;
    return false;
  }

  std::cout << "Received " << nodes.size() << " storage nodes" << std::endl;

  // Загрузка чанков и сохранение информации о узлах
  std::cout << "Uploading chunks..." << std::endl;
  size_t uploadedCount = 0;

  // Структура для хранения информации о загруженных чанках с nodeIds
  struct ChunkWithNodes {
    Chunk chunk;
    std::vector<std::string> nodeIds;
  };
  std::vector<ChunkWithNodes> chunksWithNodes;

  for (size_t i = 0; i < chunks.size(); ++i) {
    std::vector<std::string> uploadedNodeIds;
    if (!UploadChunk(chunks[i], nodes, uploadedNodeIds)) {
      std::cerr << "Error: Failed to upload chunk " << i << std::endl;
      return false;
    }

    ChunkWithNodes chunkWithNodes;
    chunkWithNodes.chunk = chunks[i];
    chunkWithNodes.nodeIds = uploadedNodeIds;
    chunksWithNodes.push_back(chunkWithNodes);

    uploadedCount++;
    ReportProgress(uploadedCount, chunks.size());
  }

  // Уведомление о завершении
  std::cout << "Notifying metadata server about upload completion..."
            << std::endl;

  // Подготовка nodeIds для каждого чанка
  std::vector<std::vector<std::string>> chunkNodeIds;
  for (const auto &chunkWithNodes : chunksWithNodes) {
    chunkNodeIds.push_back(chunkWithNodes.nodeIds);
  }

  if (!metadataClient->NotifyUploadComplete(remoteFilename, chunks,
                                            chunkNodeIds)) {
    std::cerr << "Error: Failed to notify upload completion" << std::endl;
    return false;
  }

  std::cout << "File uploaded successfully!" << std::endl;
  return true;
}

