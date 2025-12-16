#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

struct StorageNode {
  std::string nodeId; // Уникальный идентификатор
  std::string ipAddress; // IPv4 адрес
  int port; // Порт узла
  uint64_t freeSpace; // Свободное место в байтах
  uint64_t totalSpace; // Общее место в байтах
  std::chrono::time_point<std::chrono::steady_clock> lastSeen;
  std::chrono::time_point<std::chrono::steady_clock> registeredAt;
  bool isActive; // Флаг активности

  // Статистика
  size_t chunksStored; // Количество хранимых чанков
  uint64_t bytesStored; // Объём хранимых данных

  // Методы валидации
  bool IsValid() const;
  bool IsActive() const;
};

class NodeManager {
private:
  std::unordered_map<std::string, StorageNode> nodes;
  mutable std::mutex nodesMutex; // mutable для использования в const методах
  std::thread keepAliveThread;
  std::atomic<bool> running;

  // Конфигурация
  static const int KEEP_ALIVE_INTERVAL_SEC = 30;
  static const int NODE_TIMEOUT_SEC = 60;
  static const int MAX_NODES = 1000;

public:
  NodeManager();
  ~NodeManager();

  // Регистрация узлов
  bool RegisterNode(const std::string &ip, int port, uint64_t freeSpace,
                   std::string &nodeId);
  bool UnregisterNode(const std::string &nodeId);
  bool UpdateNodeSpace(const std::string &nodeId, uint64_t freeSpace);
  void UpdateNodeLastSeen(const std::string &nodeId);

  // Получение информации
  StorageNode *GetNode(const std::string &nodeId);
  std::vector<StorageNode> GetAvailableNodes(size_t count,
                                             uint64_t requiredSpace);
  std::vector<StorageNode> GetAllActiveNodes();
  size_t GetActiveNodeCount();

  // Мониторинг
  void StartKeepAliveChecker();
  void StopKeepAliveChecker();
  void CheckNodeHealth();

  // Статистика
  size_t GetTotalNodes() const;
  size_t GetActiveNodes() const;
  uint64_t GetTotalFreeSpace() const;

private:
  // Внутренние методы
  std::string GenerateNodeId();
  bool ValidateNodeInfo(const std::string &ip, int port, uint64_t freeSpace);
  void RemoveInactiveNodes();
  std::vector<StorageNode> FilterAndSortNodes(uint64_t requiredSpace);
};

