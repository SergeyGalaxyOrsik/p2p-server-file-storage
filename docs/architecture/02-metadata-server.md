# Архитектура Metadata Server

## 1. Назначение и ответственность

Metadata Server является центральным координатором системы CourseStore. Он отвечает за:
- Регистрацию и мониторинг узлов хранения
- Хранение метаданных о файлах и их чанках
- Координацию процесса загрузки и скачивания файлов

## 2. Структура компонентов

### 2.1. Основные модули

```
metadata-server/
├── src/
│   ├── main.cpp                 # Точка входа
│   ├── server.cpp               # Основной класс сервера
│   ├── node_manager.cpp         # Управление узлами
│   ├── metadata_manager.cpp     # Управление метаданными
│   ├── protocol_handler.cpp     # Обработка протокола
│   └── network_utils.cpp        # Сетевые утилиты
├── include/
│   ├── server.h
│   ├── node_manager.h
│   ├── metadata_manager.h
│   ├── protocol_handler.h
│   ├── network_utils.h
│   └── protocol.h               # Определения протокола
└── CMakeLists.txt
```

## 3. Классовая структура

### 3.1. Класс `MetadataServer`

**Ответственность:** Основной класс сервера, управляющий жизненным циклом

```cpp
class MetadataServer {
private:
    SOCKET listenSocket;
    NodeManager nodeManager;
    MetadataManager metadataManager;
    ProtocolHandler protocolHandler;
    bool running;
    
public:
    bool Initialize(int port);
    void Run();
    void Shutdown();
    void HandleClient(SOCKET clientSocket);
};
```

### 3.2. Класс `NodeManager`

**Ответственность:** Управление узлами хранения

```cpp
struct StorageNode {
    std::string nodeId;           // UUID узла
    std::string ipAddress;        // IP-адрес
    int port;                     // Порт
    uint64_t freeSpace;           // Свободное место (байты)
    std::chrono::time_point<std::chrono::steady_clock> lastSeen;
    bool isActive;
};

class NodeManager {
private:
    std::unordered_map<std::string, StorageNode> nodes;
    std::mutex nodesMutex;
    std::thread keepAliveThread;
    
public:
    bool RegisterNode(const std::string& ip, int port, 
                     uint64_t freeSpace, std::string& nodeId);
    bool UnregisterNode(const std::string& nodeId);
    std::vector<StorageNode> GetAvailableNodes(size_t count, 
                                               uint64_t requiredSpace);
    void UpdateNodeLastSeen(const std::string& nodeId);
    void CheckNodeHealth();
    void StartKeepAliveChecker();
    void StopKeepAliveChecker();
};
```

**Алгоритм выбора узлов:**
1. Фильтрация активных узлов
2. Сортировка по свободному месту (по убыванию)
3. Выбор первых N узлов с достаточным свободным местом
4. Распределение чанков с учётом балансировки нагрузки

### 3.3. Класс `MetadataManager`

**Ответственность:** Управление метаданными файлов

```cpp
struct ChunkInfo {
    std::string chunkId;          // SHA-256 хеш чанка
    std::vector<std::string> nodeIds;  // Узлы, хранящие чанк
    size_t size;                  // Размер чанка
    size_t index;                  // Порядковый номер в файле
};

struct FileMetadata {
    std::string filename;
    uint64_t totalSize;
    std::vector<ChunkInfo> chunks;
    std::chrono::time_point<std::chrono::steady_clock> uploadTime;
};

class MetadataManager {
private:
    std::unordered_map<std::string, FileMetadata> files;
    std::mutex filesMutex;
    
public:
    bool RegisterFile(const std::string& filename, 
                     uint64_t size,
                     const std::vector<ChunkInfo>& chunks);
    FileMetadata* GetFileMetadata(const std::string& filename);
    bool DeleteFile(const std::string& filename);
    std::vector<std::string> ListFiles();
    void SaveToFile(const std::string& path);  // Опционально: JSON
    bool LoadFromFile(const std::string& path); // Опционально: JSON
};
```

**Структура данных:**
```
files: {
    "document.pdf": {
        filename: "document.pdf",
        totalSize: 5242880,
        chunks: [
            {
                chunkId: "abc123...",
                nodeIds: ["node1", "node2"],
                size: 1048576,
                index: 0
            },
            ...
        ]
    }
}
```

## 4. Сетевой слой

### 4.1. Инициализация Winsock

```cpp
bool InitializeWinsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    return result == 0;
}
```

### 4.2. Основной цикл сервера

```cpp
void MetadataServer::Run() {
    while (running) {
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket != INVALID_SOCKET) {
            // Обработка в отдельном потоке или асинхронно
            std::thread(&MetadataServer::HandleClient, this, clientSocket).detach();
        }
    }
}
```

### 4.3. Обработка клиентских запросов

```cpp
void MetadataServer::HandleClient(SOCKET clientSocket) {
    std::string request = ReceiveMessage(clientSocket);
    std::string response = protocolHandler.ProcessRequest(request);
    SendMessage(clientSocket, response);
    closesocket(clientSocket);
}
```

## 5. Протокол взаимодействия

### 5.1. Запросы от Storage Node

- `REGISTER_NODE <ip> <port> <free_space>` - Регистрация узла
- `KEEP_ALIVE <node_id>` - Подтверждение активности
- `UPDATE_SPACE <node_id> <free_space>` - Обновление свободного места

### 5.2. Запросы от Client

- `REQUEST_UPLOAD <filename> <size>` - Запрос на загрузку
- `UPLOAD_COMPLETE <filename> <chunk_map>` - Уведомление о завершении
- `REQUEST_DOWNLOAD <filename>` - Запрос на скачивание
- `LIST_FILES` - Список файлов

## 6. Мониторинг и обслуживание

### 6.1. Keep-Alive механизм

```cpp
void NodeManager::StartKeepAliveChecker() {
    keepAliveThread = std::thread([this]() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            CheckNodeHealth();
        }
    });
}

void NodeManager::CheckNodeHealth() {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(nodesMutex);
    
    for (auto it = nodes.begin(); it != nodes.end();) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.lastSeen).count();
        
        if (elapsed > 60) {  // 60 секунд таймаут
            it = nodes.erase(it);
        } else {
            ++it;
        }
    }
}
```

### 6.2. Логирование

- Регистрация/отключение узлов
- Загрузка/скачивание файлов
- Ошибки обработки запросов

## 7. Обработка ошибок

- Некорректные запросы → ответ с кодом ошибки
- Недостаточно узлов → ошибка при загрузке
- Файл не найден → ошибка при скачивании
- Сетевые ошибки → логирование и закрытие соединения

## 8. Опциональные улучшения

- Сохранение метаданных в JSON при завершении
- Загрузка метаданных при старте
- Метрики производительности
- REST API вместо текстового протокола

