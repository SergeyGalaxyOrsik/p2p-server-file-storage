# Детальная архитектура Metadata Server

## 1. Обзор компонентов

Metadata Server состоит из следующих основных компонентов:

```
MetadataServer (главный класс)
├── NodeManager (управление узлами)
├── MetadataManager (управление метаданными)
├── ProtocolHandler (обработка протокола)
└── NetworkLayer (сетевой слой)
```

## 2. Детальная структура классов

### 2.1. Класс MetadataServer

**Назначение:** Главный класс, координирующий работу всех компонентов сервера.

**Полная сигнатура:**

```cpp
class MetadataServer {
private:
    // Сетевые компоненты
    SOCKET listenSocket;
    int port;
    bool running;
    
    // Менеджеры
    NodeManager nodeManager;
    MetadataManager metadataManager;
    ProtocolHandler protocolHandler;
    
    // Потоки
    std::thread acceptThread;
    std::vector<std::thread> clientThreads;
    
    // Конфигурация
    static const int MAX_CLIENTS = 100;
    static const int SOCKET_TIMEOUT_SEC = 30;
    
public:
    MetadataServer(int port = 8080);
    ~MetadataServer();
    
    // Инициализация и запуск
    bool Initialize();
    void Run();
    void Shutdown();
    
    // Обработка клиентов
    void HandleClient(SOCKET clientSocket);
    static void ClientHandlerThread(MetadataServer* server, SOCKET clientSocket);
    
    // Геттеры
    NodeManager& GetNodeManager() { return nodeManager; }
    MetadataManager& GetMetadataManager() { return metadataManager; }
    
private:
    // Внутренние методы
    bool InitializeWinsock();
    bool CreateListenSocket();
    void AcceptLoop();
    void Cleanup();
};
```

**Жизненный цикл:**

1. **Инициализация:**
   - Инициализация Winsock
   - Создание слушающего сокета
   - Привязка к порту
   - Запуск NodeManager и MetadataManager

2. **Работа:**
   - Основной цикл приёма соединений
   - Создание потоков для обработки клиентов
   - Мониторинг состояния

3. **Завершение:**
   - Корректное закрытие всех соединений
   - Остановка потоков
   - Очистка ресурсов

### 2.2. Класс NodeManager

**Назначение:** Управление жизненным циклом узлов хранения.

**Полная сигнатура:**

```cpp
struct StorageNode {
    std::string nodeId;              // Уникальный идентификатор
    std::string ipAddress;           // IPv4 адрес
    int port;                        // Порт узла
    uint64_t freeSpace;              // Свободное место в байтах
    uint64_t totalSpace;             // Общее место в байтах
    std::chrono::time_point<std::chrono::steady_clock> lastSeen;
    std::chrono::time_point<std::chrono::steady_clock> registeredAt;
    bool isActive;                   // Флаг активности
    
    // Статистика
    size_t chunksStored;              // Количество хранимых чанков
    uint64_t bytesStored;             // Объём хранимых данных
};

class NodeManager {
private:
    std::unordered_map<std::string, StorageNode> nodes;
    std::mutex nodesMutex;
    std::thread keepAliveThread;
    bool running;
    
    // Конфигурация
    static const int KEEP_ALIVE_INTERVAL_SEC = 30;
    static const int NODE_TIMEOUT_SEC = 60;
    static const int MAX_NODES = 1000;
    
public:
    NodeManager();
    ~NodeManager();
    
    // Регистрация узлов
    bool RegisterNode(const std::string& ip, int port, 
                     uint64_t freeSpace, std::string& nodeId);
    bool UnregisterNode(const std::string& nodeId);
    bool UpdateNodeSpace(const std::string& nodeId, uint64_t freeSpace);
    void UpdateNodeLastSeen(const std::string& nodeId);
    
    // Получение информации
    StorageNode* GetNode(const std::string& nodeId);
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
    bool ValidateNodeInfo(const std::string& ip, int port, uint64_t freeSpace);
    void RemoveInactiveNodes();
    std::vector<StorageNode> FilterAndSortNodes(uint64_t requiredSpace);
};
```

**Алгоритм регистрации узла:**

1. Валидация входных данных (IP, порт, свободное место)
2. Генерация уникального nodeId (UUID или хеш)
3. Создание записи StorageNode
4. Добавление в map с блокировкой мьютекса
5. Возврат nodeId клиенту

**Алгоритм выбора узлов для загрузки:**

1. Фильтрация активных узлов (isActive == true)
2. Проверка последнего контакта (lastSeen < timeout)
3. Фильтрация по свободному месту (freeSpace >= requiredSpace)
4. Сортировка по свободному месту (по убыванию)
5. Выбор первых N узлов
6. Балансировка нагрузки (распределение чанков равномерно)

**Keep-Alive механизм:**

```cpp
void NodeManager::StartKeepAliveChecker() {
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

void NodeManager::CheckNodeHealth() {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(nodesMutex);
    
    for (auto it = nodes.begin(); it != nodes.end();) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.lastSeen).count();
        
        if (elapsed > NODE_TIMEOUT_SEC) {
            // Узел неактивен, помечаем как неактивный
            it->second.isActive = false;
            
            // Можно удалить сразу или оставить для статистики
            // it = nodes.erase(it);
            ++it;
        } else {
            ++it;
        }
    }
}
```

### 2.3. Класс MetadataManager

**Назначение:** Управление метаданными файлов и их чанков.

**Полная сигнатура:**

```cpp
struct ChunkInfo {
    std::string chunkId;              // SHA-256 хеш (64 символа hex)
    size_t index;                     // Порядковый номер (0-based)
    size_t size;                      // Размер чанка в байтах
    std::vector<std::string> nodeIds; // Узлы, хранящие чанк (репликация)
    
    // Валидация
    bool IsValid() const {
        return !chunkId.empty() && 
               chunkId.length() == 64 &&
               !nodeIds.empty() &&
               size > 0;
    }
};

struct FileMetadata {
    std::string filename;              // Имя файла в системе
    uint64_t totalSize;                // Общий размер файла
    std::vector<ChunkInfo> chunks;      // Список чанков
    std::chrono::time_point<std::chrono::steady_clock> uploadTime;
    std::chrono::time_point<std::chrono::steady_clock> lastAccessed;
    
    // Валидация
    bool IsValid() const {
        return !filename.empty() && 
               totalSize > 0 &&
               !chunks.empty();
    }
    
    // Утилиты
    size_t GetChunkCount() const { return chunks.size(); }
    bool HasChunk(const std::string& chunkId) const;
};

class MetadataManager {
private:
    std::unordered_map<std::string, FileMetadata> files;
    std::mutex filesMutex;
    
    // Статистика
    size_t totalFiles;
    uint64_t totalBytes;
    
public:
    MetadataManager();
    ~MetadataManager();
    
    // Управление файлами
    bool RegisterFile(const std::string& filename, 
                     uint64_t size,
                     const std::vector<ChunkInfo>& chunks);
    bool DeleteFile(const std::string& filename);
    FileMetadata* GetFileMetadata(const std::string& filename);
    
    // Поиск и список
    std::vector<std::string> ListFiles();
    std::vector<FileMetadata> GetAllFiles();
    bool FileExists(const std::string& filename);
    
    // Работа с чанками
    std::vector<ChunkInfo> GetFileChunks(const std::string& filename);
    ChunkInfo* GetChunkInfo(const std::string& filename, 
                           const std::string& chunkId);
    
    // Сохранение/загрузка (опционально)
    void SaveToFile(const std::string& path);
    bool LoadFromFile(const std::string& path);
    
    // Статистика
    size_t GetFileCount() const;
    uint64_t GetTotalBytes() const;
    
private:
    // Внутренние методы
    bool ValidateFileMetadata(const FileMetadata& metadata);
    void UpdateStatistics();
    std::string SanitizeFilename(const std::string& filename);
};
```

**Структура данных:**

```
files: unordered_map<string, FileMetadata>
  Key: filename (например, "document.pdf")
  Value: FileMetadata {
    filename: "document.pdf"
    totalSize: 5242880
    chunks: [
      {
        chunkId: "abc123def456..."
        index: 0
        size: 1048576
        nodeIds: ["node1", "node2"]
      },
      {
        chunkId: "789ghi012jkl..."
        index: 1
        size: 1048576
        nodeIds: ["node3", "node4"]
      },
      ...
    ]
    uploadTime: <timestamp>
  }
```

**Алгоритм регистрации файла:**

1. Валидация имени файла (санитизация)
2. Проверка на дубликаты (опционально)
3. Валидация метаданных (размер, чанки)
4. Сортировка чанков по index
5. Проверка целостности (все индексы последовательны)
6. Сохранение в map с блокировкой
7. Обновление статистики

### 2.4. Класс ProtocolHandler

**Назначение:** Парсинг и обработка команд протокола.

**Полная сигнатура:**

```cpp
class ProtocolHandler {
private:
    NodeManager* nodeManager;
    MetadataManager* metadataManager;
    
    // Константы протокола
    static const std::string CMD_REGISTER_NODE;
    static const std::string CMD_KEEP_ALIVE;
    static const std::string CMD_UPDATE_SPACE;
    static const std::string CMD_REQUEST_UPLOAD;
    static const std::string CMD_UPLOAD_COMPLETE;
    static const std::string CMD_REQUEST_DOWNLOAD;
    static const std::string CMD_LIST_FILES;
    
public:
    ProtocolHandler(NodeManager* nodeManager, 
                   MetadataManager* metadataManager);
    
    // Основной метод обработки
    std::string ProcessRequest(const std::string& request, SOCKET socket);
    
private:
    // Обработчики команд от Storage Node
    std::string HandleRegisterNode(const std::vector<std::string>& args);
    std::string HandleKeepAlive(const std::vector<std::string>& args);
    std::string HandleUpdateSpace(const std::vector<std::string>& args);
    
    // Обработчики команд от Client
    std::string HandleRequestUpload(const std::vector<std::string>& args);
    std::string HandleUploadComplete(SOCKET socket);
    std::string HandleRequestDownload(const std::vector<std::string>& args);
    std::string HandleListFiles();
    
    // Утилиты
    std::vector<std::string> ParseCommand(const std::string& command);
    std::string CreateErrorResponse(const std::string& errorCode, 
                                   const std::string& message);
    std::string CreateSuccessResponse(const std::string& data);
    bool ReadMultilineRequest(SOCKET socket, std::string& request);
};
```

**Алгоритм обработки запроса:**

1. Парсинг команды (разделение по пробелам)
2. Определение типа команды
3. Валидация параметров
4. Вызов соответствующего обработчика
5. Формирование ответа
6. Возврат ответа

**Пример обработки REGISTER_NODE:**

```cpp
std::string ProtocolHandler::HandleRegisterNode(
    const std::vector<std::string>& args) {
    
    // Валидация аргументов
    if (args.size() != 4) {
        return CreateErrorResponse("INVALID_PARAMETERS", 
            "Expected: REGISTER_NODE <ip> <port> <free_space>");
    }
    
    // Парсинг параметров
    std::string ip = args[1];
    int port = std::stoi(args[2]);
    uint64_t freeSpace = std::stoull(args[3]);
    
    // Регистрация узла
    std::string nodeId;
    if (nodeManager->RegisterNode(ip, port, freeSpace, nodeId)) {
        return "REGISTER_RESPONSE OK " + nodeId + "\r\n";
    } else {
        return CreateErrorResponse("REGISTRATION_FAILED", 
            "Failed to register node");
    }
}
```

**Обработка многострочных запросов (UPLOAD_COMPLETE):**

```cpp
std::string ProtocolHandler::HandleUploadComplete(SOCKET socket) {
    std::string request;
    if (!ReadMultilineRequest(socket, request)) {
        return CreateErrorResponse("READ_ERROR", 
            "Failed to read multiline request");
    }
    
    // Парсинг первой строки: UPLOAD_COMPLETE <filename>
    std::vector<std::string> lines = SplitLines(request);
    if (lines.empty()) {
        return CreateErrorResponse("INVALID_FORMAT", 
            "Empty request");
    }
    
    std::vector<std::string> firstLine = ParseCommand(lines[0]);
    if (firstLine.size() != 2) {
        return CreateErrorResponse("INVALID_PARAMETERS", 
            "Expected: UPLOAD_COMPLETE <filename>");
    }
    
    std::string filename = firstLine[1];
    
    // Парсинг чанков
    std::vector<ChunkInfo> chunks;
    for (size_t i = 1; i < lines.size(); i++) {
        if (lines[i] == "END_CHUNKS") {
            break;
        }
        
        std::vector<std::string> chunkParts = ParseCommand(lines[i]);
        if (chunkParts.size() < 5) {
            continue; // Пропуск некорректной строки
        }
        
        ChunkInfo chunk;
        chunk.chunkId = chunkParts[0];
        chunk.index = std::stoul(chunkParts[1]);
        chunk.size = std::stoull(chunkParts[2]);
        chunk.nodeIds.push_back(chunkParts[3]);
        if (chunkParts.size() > 4) {
            chunk.nodeIds.push_back(chunkParts[4]);
        }
        
        chunks.push_back(chunk);
    }
    
    // Регистрация файла
    // Вычисление totalSize из chunks
    uint64_t totalSize = 0;
    for (const auto& chunk : chunks) {
        totalSize += chunk.size;
    }
    
    if (metadataManager->RegisterFile(filename, totalSize, chunks)) {
        return "UPLOAD_COMPLETE_RESPONSE OK\r\n";
    } else {
        return CreateErrorResponse("REGISTRATION_FAILED", 
            "Failed to register file");
    }
}
```

### 2.5. Сетевые утилиты

**Файл: network_utils.h/cpp**

```cpp
namespace NetworkUtils {
    // Инициализация Winsock
    bool InitializeWinsock();
    void CleanupWinsock();
    
    // Работа с сообщениями
    bool SendMessage(SOCKET socket, const std::string& message);
    bool ReceiveMessage(SOCKET socket, std::string& message, 
                       size_t maxSize = 4096, int timeoutSec = 30);
    
    // Работа с бинарными данными
    bool SendBinaryData(SOCKET socket, const void* data, size_t size);
    bool ReceiveBinaryData(SOCKET socket, void* buffer, size_t size, 
                          int timeoutSec = 60);
    
    // Утилиты
    std::string GetClientIP(SOCKET socket);
    bool SetSocketTimeout(SOCKET socket, int seconds);
    void CloseSocket(SOCKET socket);
}
```

**Реализация ReceiveMessage:**

```cpp
bool NetworkUtils::ReceiveMessage(SOCKET socket, std::string& message,
                                 size_t maxSize, int timeoutSec) {
    message.clear();
    
    // Установка таймаута
    SetSocketTimeout(socket, timeoutSec);
    
    char buffer[1024];
    while (message.length() < maxSize) {
        int bytesReceived = recv(socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytesReceived == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAETIMEDOUT) {
                return false; // Таймаут
            }
            return false; // Ошибка
        }
        
        if (bytesReceived == 0) {
            break; // Соединение закрыто
        }
        
        buffer[bytesReceived] = '\0';
        message += buffer;
        
        // Проверка на завершение сообщения (\r\n\r\n или просто \r\n)
        if (message.find("\r\n") != std::string::npos) {
            break;
        }
    }
    
    // Удаление \r\n в конце
    if (message.length() >= 2 && 
        message.substr(message.length() - 2) == "\r\n") {
        message = message.substr(0, message.length() - 2);
    }
    
    return true;
}
```

## 3. Потоки и синхронизация

### 3.1. Структура потоков

```
Main Thread
├── Accept Thread (приём соединений)
└── Client Handler Threads (обработка клиентов)
    └── ProtocolHandler::ProcessRequest

Keep-Alive Thread (NodeManager)
└── CheckNodeHealth (каждые 30 секунд)
```

### 3.2. Синхронизация

- **NodeManager::nodesMutex** - защита доступа к map узлов
- **MetadataManager::filesMutex** - защита доступа к map файлов
- **std::atomic<bool> running** - флаг работы сервера

### 3.3. Обработка клиентов

```cpp
void MetadataServer::HandleClient(SOCKET clientSocket) {
    try {
        std::string request;
        
        // Чтение запроса
        if (!NetworkUtils::ReceiveMessage(clientSocket, request)) {
            closesocket(clientSocket);
            return;
        }
        
        // Обработка запроса
        std::string response = protocolHandler.ProcessRequest(
            request, clientSocket);
        
        // Отправка ответа
        NetworkUtils::SendMessage(clientSocket, response);
        
    } catch (const std::exception& e) {
        // Логирование ошибки
        std::cerr << "Error handling client: " << e.what() << std::endl;
    }
    
    closesocket(clientSocket);
}
```

## 4. Обработка ошибок

### 4.1. Типы ошибок

1. **Сетевые ошибки:**
   - Таймауты
   - Разрыв соединения
   - Ошибки Winsock

2. **Ошибки протокола:**
   - Неизвестная команда
   - Некорректные параметры
   - Неверный формат

3. **Бизнес-логика:**
   - Недостаточно узлов
   - Файл не найден
   - Узел не найден

### 4.2. Стратегия обработки

- Все ошибки логируются
- Клиенту возвращается понятное сообщение об ошибке
- Соединение закрывается корректно
- Внутренние ошибки не раскрываются клиенту

## 5. Логирование

### 5.1. Уровни логирования

- **INFO** - обычные операции (регистрация узла, загрузка файла)
- **WARNING** - предупреждения (таймаут узла, неполные данные)
- **ERROR** - ошибки (сетевые ошибки, ошибки протокола)

### 5.2. Формат логов

```
[2024-01-15 10:30:45] [INFO] Node registered: node1 (192.168.1.100:9000)
[2024-01-15 10:30:50] [INFO] File uploaded: document.pdf (5242880 bytes)
[2024-01-15 10:31:00] [WARNING] Node timeout: node2 (last seen 65s ago)
[2024-01-15 10:31:05] [ERROR] Invalid command: UNKNOWN_CMD
```

## 6. Конфигурация

### 6.1. Параметры конфигурации

```cpp
struct ServerConfig {
    int port = 8080;
    int maxClients = 100;
    int socketTimeoutSec = 30;
    int keepAliveIntervalSec = 30;
    int nodeTimeoutSec = 60;
    bool enableLogging = true;
    std::string logFile = "metadata-server.log";
    std::string metadataBackupFile = "metadata_backup.json"; // опционально
};
```

### 6.2. Загрузка конфигурации

- Из командной строки (аргументы)
- Из файла конфигурации (опционально)
- Значения по умолчанию

## 7. Тестирование

### 7.1. Unit-тесты

- NodeManager: регистрация, выбор узлов, keep-alive
- MetadataManager: регистрация файлов, поиск, валидация
- ProtocolHandler: парсинг команд, формирование ответов

### 7.2. Интеграционные тесты

- Полный цикл загрузки файла
- Полный цикл скачивания файла
- Обработка отказов узлов

### 7.3. Нагрузочное тестирование

- Множественные одновременные соединения
- Большое количество узлов
- Большое количество файлов

