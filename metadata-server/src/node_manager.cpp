#include "node_manager.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

// Валидация узла
bool StorageNode::IsValid() const {
  return !nodeId.empty() && !ipAddress.empty() && port > 0 && port < 65536;
}

bool StorageNode::IsActive() const {
  return isActive;
}

// Конструктор
NodeManager::NodeManager() : running(false) {}

// Деструктор
NodeManager::~NodeManager() { StopKeepAliveChecker(); }

// Генерация уникального nodeId
std::string NodeManager::GenerateNodeId() {
  // Простая генерация на основе времени и случайного числа
  // В реальной системе можно использовать UUID
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 15);

  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch())
                  .count();

  std::stringstream ss;
  ss << std::hex << time;
  for (int i = 0; i < 8; ++i) {
    ss << std::hex << dis(gen);
  }

  return ss.str();
}

// Валидация информации об узле
bool NodeManager::ValidateNodeInfo(const std::string &ip, int port,
                                   uint64_t freeSpace) {
  // Проверка IP адреса (простая проверка)
  if (ip.empty() || ip.length() > 15) {
    return false;
  }

  // Проверка порта
  if (port <= 0 || port >= 65536) {
    return false;
  }

  // Проверка свободного места (может быть 0, но не отрицательным)
  // freeSpace может быть любым uint64_t значением

  return true;
}

// Регистрация узла
bool NodeManager::RegisterNode(const std::string &ip, int port,
                               uint64_t freeSpace, std::string &nodeId) {
  // Валидация параметров
  if (!ValidateNodeInfo(ip, port, freeSpace)) {
    return false;
  }

  // Проверка максимального количества узлов
  {
    std::lock_guard<std::mutex> lock(nodesMutex);
    if (nodes.size() >= MAX_NODES) {
      return false;
    }
  }

  // Генерация уникального nodeId
  std::string newNodeId = GenerateNodeId();

  // Проверка уникальности (маловероятно, но на всякий случай)
  {
    std::lock_guard<std::mutex> lock(nodesMutex);
    while (nodes.find(newNodeId) != nodes.end()) {
      newNodeId = GenerateNodeId();
    }
  }

  // Создание записи узла
  StorageNode node;
  node.nodeId = newNodeId;
  node.ipAddress = ip;
  node.port = port;
  node.freeSpace = freeSpace;
  node.totalSpace = freeSpace; // Пока используем freeSpace как totalSpace
  node.lastSeen = std::chrono::steady_clock::now();
  node.registeredAt = std::chrono::steady_clock::now();
  node.isActive = true;
  node.chunksStored = 0;
  node.bytesStored = 0;

  // Сохранение в map с мьютексом
  {
    std::lock_guard<std::mutex> lock(nodesMutex);
    nodes[newNodeId] = node;
  }

  nodeId = newNodeId;
  return true;
}

// Удаление узла
bool NodeManager::UnregisterNode(const std::string &nodeId) {
  std::lock_guard<std::mutex> lock(nodesMutex);
  auto it = nodes.find(nodeId);
  if (it != nodes.end()) {
    nodes.erase(it);
    return true;
  }
  return false;
}

// Обновление свободного места
bool NodeManager::UpdateNodeSpace(const std::string &nodeId,
                                  uint64_t freeSpace) {
  std::lock_guard<std::mutex> lock(nodesMutex);
  auto it = nodes.find(nodeId);
  if (it != nodes.end()) {
    it->second.freeSpace = freeSpace;
    return true;
  }
  return false;
}

// Обновление времени последнего контакта
void NodeManager::UpdateNodeLastSeen(const std::string &nodeId) {
  std::lock_guard<std::mutex> lock(nodesMutex);
  auto it = nodes.find(nodeId);
  if (it != nodes.end()) {
    it->second.lastSeen = std::chrono::steady_clock::now();
    it->second.isActive = true;
  }
}

// Получение узла по ID
StorageNode *NodeManager::GetNode(const std::string &nodeId) {
  std::lock_guard<std::mutex> lock(nodesMutex);
  auto it = nodes.find(nodeId);
  if (it != nodes.end()) {
    return &(it->second);
  }
  return nullptr;
}

// Фильтрация и сортировка узлов
std::vector<StorageNode> NodeManager::FilterAndSortNodes(uint64_t requiredSpace) {
  std::vector<StorageNode> availableNodes;
  auto now = std::chrono::steady_clock::now();

  size_t totalNodes = 0;
  size_t inactiveNodes = 0;
  size_t timeoutNodes = 0;
  size_t insufficientSpaceNodes = 0;

  {
    std::lock_guard<std::mutex> lock(nodesMutex);
    totalNodes = nodes.size();
    
    for (const auto &pair : nodes) {
      const StorageNode &node = pair.second;

      // Фильтрация активных узлов
      if (!node.isActive) {
        inactiveNodes++;
        continue;
      }

      // Проверка таймаута
      auto timeSinceLastSeen =
          std::chrono::duration_cast<std::chrono::seconds>(now - node.lastSeen)
              .count();
      if (timeSinceLastSeen > NODE_TIMEOUT_SEC) {
        timeoutNodes++;
        continue;
      }

      // Фильтрация по свободному месту
      if (node.freeSpace < requiredSpace) {
        insufficientSpaceNodes++;
        continue;
      }

      availableNodes.push_back(node);
    }
  }

  std::cout << "FilterAndSortNodes: total=" << totalNodes 
            << ", inactive=" << inactiveNodes 
            << ", timeout=" << timeoutNodes 
            << ", insufficientSpace=" << insufficientSpaceNodes 
            << ", available=" << availableNodes.size() << std::endl;

  // Сортировка по свободному месту (по убыванию)
  std::sort(availableNodes.begin(), availableNodes.end(),
            [](const StorageNode &a, const StorageNode &b) {
              return a.freeSpace > b.freeSpace;
            });

  return availableNodes;
}

// Получение доступных узлов для загрузки
std::vector<StorageNode> NodeManager::GetAvailableNodes(size_t count,
                                                        uint64_t requiredSpace) {
  std::vector<StorageNode> availableNodes = FilterAndSortNodes(requiredSpace);

  std::cout << "GetAvailableNodes: requested " << count 
            << " nodes with " << requiredSpace << " bytes free space" << std::endl;
  std::cout << "GetAvailableNodes: found " << availableNodes.size() 
            << " available nodes" << std::endl;

  // Выбор первых N узлов
  if (availableNodes.size() > count) {
    availableNodes.resize(count);
  }

  return availableNodes;
}

// Получение всех активных узлов
std::vector<StorageNode> NodeManager::GetAllActiveNodes() {
  std::vector<StorageNode> activeNodes;
  auto now = std::chrono::steady_clock::now();

  {
    std::lock_guard<std::mutex> lock(nodesMutex);
    for (const auto &pair : nodes) {
      const StorageNode &node = pair.second;

      // Проверка активности
      if (!node.isActive) {
        continue;
      }

      // Проверка таймаута
      auto timeSinceLastSeen =
          std::chrono::duration_cast<std::chrono::seconds>(now - node.lastSeen)
              .count();
      if (timeSinceLastSeen > NODE_TIMEOUT_SEC) {
        continue;
      }

      activeNodes.push_back(node);
    }
  }

  return activeNodes;
}

// Количество активных узлов
size_t NodeManager::GetActiveNodeCount() {
  return GetAllActiveNodes().size();
}

// Запуск keep-alive проверки
void NodeManager::StartKeepAliveChecker() {
  if (running) {
    return; // Уже запущен
  }

  running = true;
  keepAliveThread = std::thread([this]() {
    while (running) {
      std::this_thread::sleep_for(
          std::chrono::seconds(KEEP_ALIVE_INTERVAL_SEC));

      if (running) {
        CheckNodeHealth();
      }
    }
  });
}

// Остановка keep-alive проверки
void NodeManager::StopKeepAliveChecker() {
  if (!running) {
    return;
  }

  running = false;

  if (keepAliveThread.joinable()) {
    keepAliveThread.join();
  }
}

// Проверка здоровья узлов
void NodeManager::CheckNodeHealth() {
  auto now = std::chrono::steady_clock::now();

  {
    std::lock_guard<std::mutex> lock(nodesMutex);
    for (auto &pair : nodes) {
      StorageNode &node = pair.second;

      // Проверка таймаута
      auto timeSinceLastSeen =
          std::chrono::duration_cast<std::chrono::seconds>(now - node.lastSeen)
              .count();

      if (timeSinceLastSeen > NODE_TIMEOUT_SEC) {
        // Помечаем узел как неактивный
        node.isActive = false;
      }
    }
  }

  // Удаление неактивных узлов (опционально)
  // RemoveInactiveNodes();
}

// Удаление неактивных узлов
void NodeManager::RemoveInactiveNodes() {
  std::lock_guard<std::mutex> lock(nodesMutex);
  auto it = nodes.begin();
  while (it != nodes.end()) {
    if (!it->second.isActive) {
      it = nodes.erase(it);
    } else {
      ++it;
    }
  }
}

// Общее количество узлов
size_t NodeManager::GetTotalNodes() const {
  std::lock_guard<std::mutex> lock(nodesMutex);
  return nodes.size();
}

// Количество активных узлов
size_t NodeManager::GetActiveNodes() const {
  size_t count = 0;
  auto now = std::chrono::steady_clock::now();

  std::lock_guard<std::mutex> lock(nodesMutex);

  for (const auto &pair : nodes) {
    const StorageNode &node = pair.second;

    if (!node.isActive) {
      continue;
    }

    // Проверка таймаута
    auto timeSinceLastSeen =
        std::chrono::duration_cast<std::chrono::seconds>(now - node.lastSeen)
            .count();
    if (timeSinceLastSeen <= NODE_TIMEOUT_SEC) {
      count++;
    }
  }

  return count;
}

// Общее свободное место
uint64_t NodeManager::GetTotalFreeSpace() const {
  uint64_t total = 0;
  auto now = std::chrono::steady_clock::now();

  std::lock_guard<std::mutex> lock(nodesMutex);

  for (const auto &pair : nodes) {
    const StorageNode &node = pair.second;

    // Учитываем только активные узлы
    if (!node.isActive) {
      continue;
    }

    // Проверка таймаута
    auto timeSinceLastSeen =
        std::chrono::duration_cast<std::chrono::seconds>(now - node.lastSeen)
            .count();
    if (timeSinceLastSeen <= NODE_TIMEOUT_SEC) {
      total += node.freeSpace;
    }
  }

  return total;
}

