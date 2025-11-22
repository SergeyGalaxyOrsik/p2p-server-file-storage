# Детальная архитектура Client

## 1. Обзор архитектуры

Client является консольной утилитой для взаимодействия пользователя с системой CourseStore. Он построен как платформо-независимое приложение, использующее стандартные библиотеки C++ для работы с файлами.

### 1.1. Структура компонентов

```
Client
├── Core Components
│   ├── Client (главный класс)
│   ├── UploadManager (управление загрузкой)
│   ├── DownloadManager (управление скачиванием)
│   ├── ChunkProcessor (разбиение и сборка файлов)
│   ├── MetadataClient (взаимодействие с Metadata Server)
│   └── NodeClient (взаимодействие с Storage Nodes)
│
└── Utilities
    ├── HashUtils (вычисление хешей SHA-256)
    └── ProgressReporter (отображение прогресса)
```

## 2. Детальная структура классов

### 2.1. Класс Client

**Назначение:** Главный класс клиента, координирующий работу всех компонентов.

**Полная сигнатура:**

```cpp
class Client {
private:
    // Конфигурация Metadata Server
    std::string metadataServerIp;
    int metadataServerPort;
    
    // Компоненты
    std::unique_ptr<MetadataClient> metadataClient;
    std::unique_ptr<UploadManager> uploadManager;
    std::unique_ptr<DownloadManager> downloadManager;
    
    // Состояние
    bool initialized;
    
public:
    Client();
    ~Client();
    
    // Инициализация
    bool Initialize(const std::string& metadataServerIp, int port);
    void Shutdown();
    
    // Выполнение команд
    bool ExecuteCommand(const std::vector<std::string>& args);
    
    // Обработчики команд
    bool HandleUpload(const std::vector<std::string>& args);
    bool HandleDownload(const std::vector<std::string>& args);
    bool HandleList();
    bool HandleHelp();
    
    // Утилиты
    void PrintUsage() const;
    void PrintError(const std::string& message) const;
    void PrintInfo(const std::string& message) const;
    
private:
    // Внутренние методы
    bool ValidateCommand(const std::vector<std::string>& args);
    std::string ExpandPath(const std::string& path);
};
```

**Жизненный цикл:**

1. **Инициализация:**
   - Создание MetadataClient
   - Создание UploadManager и DownloadManager
   - Проверка подключения к Metadata Server

2. **Выполнение команд:**
   - Парсинг аргументов командной строки
   - Маршрутизация к соответствующему обработчику
   - Выполнение команды

3. **Завершение:**
   - Очистка ресурсов
   - Закрытие соединений

### 2.2. Класс ChunkProcessor

**Назначение:** Разбиение файлов на чанки и сборка обратно. Платформо-независимая реализация через std::fstream.

**Полная сигнатура:**

```cpp
struct Chunk {
    std::string chunkId;              // SHA-256 хеш (64 символа hex)
    size_t index;                     // Порядковый номер (0-based)
    size_t size;                      // Размер чанка в байтах
    std::vector<uint8_t> data;        // Данные чанка
    
    // Валидация
    bool IsValid() const {
        return !chunkId.empty() && 
               chunkId.length() == 64 &&
               size > 0 &&
               !data.empty() &&
               data.size() == size;
    }
};

class ChunkProcessor {
private:
    static const size_t CHUNK_SIZE = 1048576;  // 1 МБ (1048576 байт)
    static const size_t BUFFER_SIZE = 65536;    // 64 КБ буфер для чтения
    
public:
    // Разбиение и сборка
    static std::vector<Chunk> SplitFile(const std::string& filepath);
    static bool AssembleFile(const std::vector<Chunk>& chunks,
                            const std::string& outputPath);
    
    // Хеширование
    static std::string CalculateChunkHash(const std::vector<uint8_t>& data);
    static std::string CalculateFileHash(const std::string& filepath);
    
    // Валидация
    static bool ValidateChunk(const Chunk& chunk);
    static bool ValidateFile(const std::string& filepath,
                            const std::string& expectedHash);
    
    // Утилиты
    static size_t GetChunkSize() { return CHUNK_SIZE; }
    static size_t CalculateChunkCount(uint64_t fileSize);
    
private:
    // Внутренние методы
    static bool ReadChunk(std::ifstream& file, std::vector<uint8_t>& buffer, 
                         size_t& bytesRead);
    static bool WriteChunk(std::ofstream& file, const std::vector<uint8_t>& data);
};
```

**Реализация SplitFile (платформо-независимая):**

```cpp
std::vector<Chunk> ChunkProcessor::SplitFile(const std::string& filepath) {
    std::vector<Chunk> chunks;
    
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return chunks;
    }
    
    // Получение размера файла
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    if (fileSize == 0) {
        file.close();
        return chunks;
    }
    
    size_t index = 0;
    std::vector<uint8_t> buffer(CHUNK_SIZE);
    
    while (file.good() && !file.eof()) {
        // Чтение чанка
        file.read(reinterpret_cast<char*>(buffer.data()), CHUNK_SIZE);
        std::streamsize bytesRead = file.gcount();
        
        if (bytesRead == 0) {
            break;
        }
        
        // Создание чанка
        Chunk chunk;
        chunk.index = index;
        chunk.size = static_cast<size_t>(bytesRead);
        chunk.data.resize(chunk.size);
        std::memcpy(chunk.data.data(), buffer.data(), chunk.size);
        
        // Вычисление хеша
        chunk.chunkId = CalculateChunkHash(chunk.data);
        
        chunks.push_back(chunk);
        index++;
    }
    
    file.close();
    return chunks;
}
```

**Реализация AssembleFile:**

```cpp
bool ChunkProcessor::AssembleFile(const std::vector<Chunk>& chunks,
                                  const std::string& outputPath) {
    if (chunks.empty()) {
        return false;
    }
    
    // Сортировка чанков по индексу
    std::vector<Chunk> sortedChunks = chunks;
    std::sort(sortedChunks.begin(), sortedChunks.end(),
              [](const Chunk& a, const Chunk& b) {
                  return a.index < b.index;
              });
    
    // Проверка целостности последовательности
    for (size_t i = 0; i < sortedChunks.size(); i++) {
        if (sortedChunks[i].index != i) {
            return false;  // Пропущен чанк
        }
    }
    
    // Запись файла
    std::ofstream file(outputPath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }
    
    for (const auto& chunk : sortedChunks) {
        // Валидация чанка перед записью
        if (!ValidateChunk(chunk)) {
            file.close();
            return false;
        }
        
        file.write(reinterpret_cast<const char*>(chunk.data.data()),
                  chunk.data.size());
        
        if (!file.good()) {
            file.close();
            return false;
        }
    }
    
    file.close();
    return true;
}
```

**Валидация чанка:**

```cpp
bool ChunkProcessor::ValidateChunk(const Chunk& chunk) {
    if (!chunk.IsValid()) {
        return false;
    }
    
    // Проверка хеша
    std::string calculatedHash = CalculateChunkHash(chunk.data);
    if (calculatedHash != chunk.chunkId) {
        return false;  // Хеш не совпадает
    }
    
    return true;
}
```

### 2.3. Класс UploadManager

**Назначение:** Управление процессом загрузки файла в систему.

**Полная сигнатура:**

```cpp
struct StorageNodeInfo {
    std::string nodeId;
    std::string ipAddress;
    int port;
    uint64_t freeSpace;
};

class UploadManager {
private:
    MetadataClient* metadataClient;
    std::unique_ptr<NodeClient> nodeClient;
    ChunkProcessor chunkProcessor;
    
    // Конфигурация
    static const int REPLICATION_FACTOR = 2;  // Коэффициент репликации
    static const int MAX_RETRIES = 3;         // Максимум попыток при ошибке
    static const int RETRY_DELAY_MS = 1000;   // Задержка между попытками
    
    // Прогресс
    std::function<void(size_t current, size_t total)> progressCallback;
    
public:
    UploadManager(MetadataClient* metadataClient);
    
    // Загрузка файла
    bool UploadFile(const std::string& localPath, 
                   const std::string& remoteFilename);
    
    // Настройка прогресса
    void SetProgressCallback(std::function<void(size_t, size_t)> callback);
    
private:
    // Внутренние методы
    bool UploadChunk(const Chunk& chunk, 
                    const std::vector<StorageNodeInfo>& nodes);
    bool UploadChunkToNode(const Chunk& chunk,
                          const StorageNodeInfo& node);
    bool NotifyUploadComplete(const std::string& filename,
                             const std::vector<Chunk>& chunks);
    std::vector<StorageNodeInfo> SelectNodesForChunk(
        const std::vector<StorageNodeInfo>& availableNodes,
        size_t chunkIndex);
    void ReportProgress(size_t current, size_t total);
};
```

**Алгоритм загрузки:**

```cpp
bool UploadManager::UploadFile(const std::string& localPath,
                              const std::string& remoteFilename) {
    // 1. Проверка существования файла
    std::ifstream testFile(localPath, std::ios::binary);
    if (!testFile.is_open()) {
        std::cerr << "Error: File not found: " << localPath << std::endl;
        return false;
    }
    testFile.close();
    
    // 2. Разбиение файла на чанки
    std::cout << "Splitting file into chunks..." << std::endl;
    std::vector<Chunk> chunks = chunkProcessor.SplitFile(localPath);
    if (chunks.empty()) {
        std::cerr << "Error: Failed to split file or file is empty" << std::endl;
        return false;
    }
    
    std::cout << "File split into " << chunks.size() << " chunks" << std::endl;
    
    // 3. Вычисление общего размера
    uint64_t totalSize = 0;
    for (const auto& chunk : chunks) {
        totalSize += chunk.size;
    }
    
    // 4. Запрос узлов у Metadata Server
    std::cout << "Requesting storage nodes from metadata server..." << std::endl;
    size_t requiredNodes = chunks.size() * REPLICATION_FACTOR;
    std::vector<StorageNodeInfo> nodes = metadataClient->RequestUploadNodes(
        remoteFilename, totalSize, requiredNodes);
    
    if (nodes.size() < requiredNodes) {
        std::cerr << "Error: Not enough storage nodes available. "
                  << "Required: " << requiredNodes 
                  << ", Available: " << nodes.size() << std::endl;
        return false;
    }
    
    std::cout << "Received " << nodes.size() << " storage nodes" << std::endl;
    
    // 5. Загрузка каждого чанка с репликацией
    std::cout << "Uploading chunks..." << std::endl;
    for (size_t i = 0; i < chunks.size(); i++) {
        const auto& chunk = chunks[i];
        
        // Выбор узлов для этого чанка
        std::vector<StorageNodeInfo> chunkNodes = SelectNodesForChunk(nodes, i);
        
        // Загрузка чанка на все узлы (репликация)
        bool success = UploadChunk(chunk, chunkNodes);
        
        if (!success) {
            std::cerr << "Error: Failed to upload chunk " << chunk.chunkId 
                      << " (index " << chunk.index << ")" << std::endl;
            return false;
        }
        
        ReportProgress(i + 1, chunks.size());
    }
    
    // 6. Уведомление Metadata Server о завершении
    std::cout << "Notifying metadata server of upload completion..." << std::endl;
    if (!NotifyUploadComplete(remoteFilename, chunks)) {
        std::cerr << "Error: Failed to notify metadata server" << std::endl;
        return false;
    }
    
    std::cout << "Upload completed successfully!" << std::endl;
    return true;
}
```

**Загрузка чанка с репликацией:**

```cpp
bool UploadManager::UploadChunk(const Chunk& chunk,
                               const std::vector<StorageNodeInfo>& nodes) {
    // Загрузка на все узлы (репликация)
    for (const auto& node : nodes) {
        int retries = 0;
        bool success = false;
        
        while (retries < MAX_RETRIES && !success) {
            success = UploadChunkToNode(chunk, node);
            
            if (!success) {
                retries++;
                if (retries < MAX_RETRIES) {
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(RETRY_DELAY_MS));
                }
            }
        }
        
        if (!success) {
            std::cerr << "Error: Failed to upload chunk to node " 
                      << node.nodeId << " after " << MAX_RETRIES 
                      << " attempts" << std::endl;
            return false;
        }
    }
    
    return true;
}
```

### 2.4. Класс DownloadManager

**Назначение:** Управление процессом скачивания файла из системы.

**Полная сигнатура:**

```cpp
class DownloadManager {
private:
    MetadataClient* metadataClient;
    std::unique_ptr<NodeClient> nodeClient;
    ChunkProcessor chunkProcessor;
    
    // Конфигурация
    static const int MAX_PARALLEL_DOWNLOADS = 10;  // Максимум параллельных загрузок
    static const int MAX_RETRIES = 3;               // Максимум попыток
    static const int RETRY_DELAY_MS = 1000;         // Задержка между попытками
    
    // Прогресс
    std::function<void(size_t current, size_t total)> progressCallback;
    std::mutex progressMutex;
    
public:
    DownloadManager(MetadataClient* metadataClient);
    
    // Скачивание файла
    bool DownloadFile(const std::string& remoteFilename,
                    const std::string& localPath);
    
    // Настройка прогресса
    void SetProgressCallback(std::function<void(size_t, size_t)> callback);
    
private:
    // Внутренние методы
    bool DownloadChunk(const ChunkInfo& chunkInfo, Chunk& chunk);
    Chunk DownloadChunkFromNode(const std::string& chunkId,
                               const StorageNodeInfo& node);
    bool TryDownloadFromNodes(const std::string& chunkId,
                             const std::vector<std::string>& nodeIds,
                             Chunk& chunk);
    void ReportProgress(size_t current, size_t total);
    StorageNodeInfo GetNodeInfo(const std::string& nodeId,
                               const FileMetadata& metadata);
};
```

**Алгоритм скачивания:**

```cpp
bool DownloadManager::DownloadFile(const std::string& remoteFilename,
                                  const std::string& localPath) {
    // 1. Запрос метаданных файла
    std::cout << "Requesting file metadata..." << std::endl;
    FileMetadata metadata = metadataClient->RequestDownload(remoteFilename);
    if (metadata.filename.empty()) {
        std::cerr << "Error: File not found: " << remoteFilename << std::endl;
        return false;
    }
    
    std::cout << "File found: " << metadata.filename 
              << " (" << metadata.totalSize << " bytes, "
              << metadata.chunks.size() << " chunks)" << std::endl;
    
    // 2. Параллельное скачивание чанков
    std::cout << "Downloading chunks..." << std::endl;
    std::vector<Chunk> chunks;
    std::mutex chunksMutex;
    std::vector<std::thread> threads;
    std::atomic<size_t> completedChunks(0);
    std::atomic<size_t> failedChunks(0);
    
    // Ограничение количества параллельных потоков
    std::queue<size_t> chunkQueue;
    for (size_t i = 0; i < metadata.chunks.size(); i++) {
        chunkQueue.push(i);
    }
    
    std::mutex queueMutex;
    size_t activeThreads = 0;
    const size_t maxThreads = std::min(
        static_cast<size_t>(MAX_PARALLEL_DOWNLOADS),
        metadata.chunks.size()
    );
    
    // Запуск потоков
    for (size_t t = 0; t < maxThreads; t++) {
        threads.emplace_back([&]() {
            while (true) {
                size_t chunkIndex;
                {
                    std::lock_guard<std::mutex> lock(queueMutex);
                    if (chunkQueue.empty()) {
                        break;
                    }
                    chunkIndex = chunkQueue.front();
                    chunkQueue.pop();
                    activeThreads++;
                }
                
                const auto& chunkInfo = metadata.chunks[chunkIndex];
                Chunk chunk;
                
                if (DownloadChunk(chunkInfo, chunk)) {
                    std::lock_guard<std::mutex> lock(chunksMutex);
                    chunks.push_back(chunk);
                    completedChunks++;
                } else {
                    failedChunks++;
                }
                
                {
                    std::lock_guard<std::mutex> lock(queueMutex);
                    activeThreads--;
                }
                
                ReportProgress(completedChunks + failedChunks, 
                              metadata.chunks.size());
            }
        });
    }
    
    // Ожидание завершения всех потоков
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 3. Проверка успешности скачивания
    if (chunks.size() != metadata.chunks.size()) {
        std::cerr << "Error: Failed to download all chunks. "
                  << "Downloaded: " << chunks.size() 
                  << ", Required: " << metadata.chunks.size() << std::endl;
        return false;
    }
    
    // 4. Сборка файла
    std::cout << "Assembling file..." << std::endl;
    if (!chunkProcessor.AssembleFile(chunks, localPath)) {
        std::cerr << "Error: Failed to assemble file" << std::endl;
        return false;
    }
    
    std::cout << "Download completed successfully!" << std::endl;
    return true;
}
```

**Скачивание чанка с отказоустойчивостью:**

```cpp
bool DownloadManager::DownloadChunk(const ChunkInfo& chunkInfo, Chunk& chunk) {
    // Попытка скачать с любого доступного узла
    return TryDownloadFromNodes(chunkInfo.chunkId, chunkInfo.nodeIds, chunk);
}

bool DownloadManager::TryDownloadFromNodes(
    const std::string& chunkId,
    const std::vector<std::string>& nodeIds,
    Chunk& chunk) {
    
    // Получение информации об узлах из метаданных
    // (в реальной реализации нужно хранить информацию об узлах)
    
    for (const auto& nodeId : nodeIds) {
        // Получение информации об узле
        StorageNodeInfo node = GetNodeInfo(nodeId, /* metadata */);
        
        if (node.ipAddress.empty()) {
            continue;  // Узел недоступен
        }
        
        // Попытка скачать с узла
        Chunk downloadedChunk = DownloadChunkFromNode(chunkId, node);
        
        if (!downloadedChunk.data.empty()) {
            // Валидация чанка
            if (ChunkProcessor::ValidateChunk(downloadedChunk) &&
                downloadedChunk.chunkId == chunkId) {
                chunk = downloadedChunk;
                return true;
            }
        }
    }
    
    return false;
}
```

### 2.5. Класс MetadataClient

**Назначение:** Взаимодействие с Metadata Server.

**Полная сигнатура:**

```cpp
struct FileMetadata {
    std::string filename;
    uint64_t totalSize;
    std::vector<ChunkInfo> chunks;
};

struct ChunkInfo {
    std::string chunkId;
    size_t index;
    size_t size;
    std::vector<std::string> nodeIds;  // Узлы, хранящие чанк
};

class MetadataClient {
private:
    std::string serverIp;
    int serverPort;
    
    // Сетевые утилиты (из common)
    // Использует NetworkUtils
    
public:
    MetadataClient(const std::string& ip, int port);
    ~MetadataClient();
    
    // Загрузка
    std::vector<StorageNodeInfo> RequestUploadNodes(
        const std::string& filename, uint64_t size, size_t nodeCount);
    bool NotifyUploadComplete(const std::string& filename,
                             const std::vector<Chunk>& chunks);
    
    // Скачивание
    FileMetadata RequestDownload(const std::string& filename);
    
    // Утилиты
    std::vector<std::string> ListFiles();
    bool TestConnection();
    
private:
    // Внутренние методы
    bool ConnectToServer(SOCKET& socket);
    bool SendRequest(SOCKET socket, const std::string& request);
    bool ReceiveResponse(SOCKET socket, std::string& response);
    bool ParseUploadResponse(const std::string& response,
                            std::vector<StorageNodeInfo>& nodes);
    bool ParseDownloadResponse(const std::string& response,
                              FileMetadata& metadata);
    bool ParseListResponse(const std::string& response,
                         std::vector<std::string>& files);
};
```

**Реализация RequestUploadNodes:**

```cpp
std::vector<StorageNodeInfo> MetadataClient::RequestUploadNodes(
    const std::string& filename, uint64_t size, size_t nodeCount) {
    
    std::vector<StorageNodeInfo> nodes;
    SOCKET sock = INVALID_SOCKET;
    
    if (!ConnectToServer(sock)) {
        return nodes;
    }
    
    // Формирование запроса
    std::string request = "REQUEST_UPLOAD " + filename + " " + 
                         std::to_string(size) + "\r\n";
    
    // Отправка запроса
    if (!NetworkUtils::SendMessage(sock, request)) {
        closesocket(sock);
        return nodes;
    }
    
    // Получение ответа
    std::string response;
    if (!NetworkUtils::ReceiveMessage(sock, response)) {
        closesocket(sock);
        return nodes;
    }
    
    closesocket(sock);
    
    // Парсинг ответа
    ParseUploadResponse(response, nodes);
    
    return nodes;
}
```

### 2.6. Класс NodeClient

**Назначение:** Взаимодействие с Storage Nodes.

**Полная сигнатура:**

```cpp
class NodeClient {
private:
    // Конфигурация
    static const int CONNECTION_TIMEOUT_SEC = 10;
    static const int DATA_TIMEOUT_SEC = 60;
    
public:
    NodeClient();
    ~NodeClient();
    
    // Работа с чанками
    bool StoreChunk(const StorageNodeInfo& node,
                   const std::string& chunkId,
                   const std::vector<uint8_t>& data);
    bool GetChunk(const StorageNodeInfo& node,
                 const std::string& chunkId,
                 std::vector<uint8_t>& data);
    bool CheckChunk(const StorageNodeInfo& node,
                   const std::string& chunkId);
    
private:
    // Внутренние методы
    bool ConnectToNode(const StorageNodeInfo& node, SOCKET& socket);
    bool SendStoreChunkRequest(SOCKET socket, const std::string& chunkId,
                              size_t size);
    bool SendGetChunkRequest(SOCKET socket, const std::string& chunkId);
    bool ReceiveBinaryData(SOCKET socket, std::vector<uint8_t>& data, 
                          size_t size);
    bool SendBinaryData(SOCKET socket, const std::vector<uint8_t>& data);
};
```

**Реализация StoreChunk:**

```cpp
bool NodeClient::StoreChunk(const StorageNodeInfo& node,
                           const std::string& chunkId,
                           const std::vector<uint8_t>& data) {
    SOCKET sock = INVALID_SOCKET;
    
    if (!ConnectToNode(node, sock)) {
        return false;
    }
    
    // Отправка команды STORE_CHUNK
    std::string command = "STORE_CHUNK " + chunkId + " " + 
                         std::to_string(data.size()) + "\r\n";
    
    if (!NetworkUtils::SendMessage(sock, command)) {
        closesocket(sock);
        return false;
    }
    
    // Отправка бинарных данных
    if (!SendBinaryData(sock, data)) {
        closesocket(sock);
        return false;
    }
    
    // Получение ответа
    std::string response;
    if (!NetworkUtils::ReceiveMessage(sock, response)) {
        closesocket(sock);
        return false;
    }
    
    closesocket(sock);
    
    // Проверка ответа
    return response.find("STORE_RESPONSE OK") != std::string::npos;
}
```

**Реализация GetChunk:**

```cpp
bool NodeClient::GetChunk(const StorageNodeInfo& node,
                         const std::string& chunkId,
                         std::vector<uint8_t>& data) {
    SOCKET sock = INVALID_SOCKET;
    
    if (!ConnectToNode(node, sock)) {
        return false;
    }
    
    // Отправка команды GET_CHUNK
    std::string command = "GET_CHUNK " + chunkId + "\r\n";
    
    if (!NetworkUtils::SendMessage(sock, command)) {
        closesocket(sock);
        return false;
    }
    
    // Получение ответа с размером
    std::string response;
    if (!NetworkUtils::ReceiveMessage(sock, response)) {
        closesocket(sock);
        return false;
    }
    
    // Парсинг размера из ответа: "GET_RESPONSE OK <size>"
    std::vector<std::string> parts = SplitString(response, ' ');
    if (parts.size() < 3 || parts[0] != "GET_RESPONSE" || parts[1] != "OK") {
        closesocket(sock);
        return false;
    }
    
    size_t size = std::stoull(parts[2]);
    
    // Получение бинарных данных
    if (!ReceiveBinaryData(sock, data, size)) {
        closesocket(sock);
        return false;
    }
    
    closesocket(sock);
    return true;
}
```

## 3. Утилиты

### 3.1. HashUtils

**Назначение:** Вычисление хешей SHA-256 для чанков.

```cpp
namespace HashUtils {
    std::string CalculateSHA256(const std::vector<uint8_t>& data);
    std::string CalculateSHA256(const std::string& filepath);
    bool VerifyHash(const std::vector<uint8_t>& data, 
                   const std::string& expectedHash);
}
```

**Реализация (используя CryptoAPI на Windows или OpenSSL на Linux):**

```cpp
std::string HashUtils::CalculateSHA256(const std::vector<uint8_t>& data) {
    // Реализация через CryptoAPI (Windows) или OpenSSL (Linux)
    // Или использовать библиотеку из common
    // ...
}
```

### 3.2. ProgressReporter

**Назначение:** Отображение прогресса операций.

```cpp
class ProgressReporter {
public:
    static void ReportUploadProgress(size_t current, size_t total);
    static void ReportDownloadProgress(size_t current, size_t total);
    static void PrintProgressBar(size_t current, size_t total, 
                                const std::string& operation);
};
```

## 4. Интерфейс командной строки

### 4.1. Формат команд

```
coursestore-client [options] <command> [arguments]
```

### 4.2. Опции

- `--server <ip>` - IP-адрес Metadata Server (по умолчанию: 127.0.0.1)
- `--port <port>` - Порт Metadata Server (по умолчанию: 8080)
- `--verbose` - Подробный вывод
- `--quiet` - Минимальный вывод

### 4.3. Команды

- `upload <local_path> <remote_filename>` - Загрузка файла
- `download <remote_filename> <local_path>` - Скачивание файла
- `list` - Список файлов в системе
- `help [command]` - Справка

### 4.4. Примеры использования

```bash
# Загрузка файла
coursestore-client upload document.pdf document.pdf

# Скачивание файла
coursestore-client download document.pdf ./downloaded.pdf

# Список файлов
coursestore-client list

# С указанием сервера
coursestore-client --server 192.168.1.100 --port 8080 upload file.txt file.txt
```

## 5. Обработка ошибок

### 5.1. Типы ошибок

1. **Ошибки файловой системы:**
   - Файл не найден
   - Ошибка чтения/записи
   - Недостаточно места на диске

2. **Сетевые ошибки:**
   - Не удаётся подключиться к Metadata Server
   - Не удаётся подключиться к Storage Node
   - Таймауты
   - Разрыв соединения

3. **Ошибки протокола:**
   - Некорректный ответ от сервера
   - Ошибка парсинга

4. **Бизнес-логика:**
   - Файл не найден в системе
   - Недостаточно узлов
   - Не удалось скачать все чанки

### 5.2. Стратегия обработки

- Все ошибки логируются с деталями
- Пользователю показываются понятные сообщения
- Повторные попытки при сетевых ошибках
- Частичное восстановление при скачивании (если доступны реплики)

## 6. Валидация целостности

### 6.1. Проверка хешей чанков

Каждый скачанный чанк проверяется на соответствие его chunkId:

```cpp
bool ValidateDownloadedChunk(const Chunk& chunk) {
    std::string calculatedHash = ChunkProcessor::CalculateChunkHash(chunk.data);
    return calculatedHash == chunk.chunkId;
}
```

### 6.2. Проверка последовательности

При сборке файла проверяется, что все чанки присутствуют и в правильном порядке.

## 7. Оптимизации

### 7.1. Параллельная загрузка

Можно загружать несколько чанков параллельно для ускорения процесса.

### 7.2. Параллельное скачивание

Скачивание всех чанков параллельно (с ограничением количества потоков).

### 7.3. Буферизация

Использование буферов при чтении/записи файлов для повышения производительности.

## 8. Логирование

- Прогресс операций (процент завершения)
- Информация о подключении к серверам и узлам
- Детали ошибок
- Статистика (скорость передачи, время выполнения)

## 9. Структура проекта

```
client/
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── client.cpp
│   │   ├── upload_manager.cpp
│   │   ├── download_manager.cpp
│   │   ├── chunk_processor.cpp
│   │   ├── metadata_client.cpp
│   │   └── node_client.cpp
│   └── utils/
│       ├── hash_utils.cpp
│       └── progress_reporter.cpp
│
├── include/
│   ├── core/
│   │   ├── client.h
│   │   ├── upload_manager.h
│   │   ├── download_manager.h
│   │   ├── chunk_processor.h
│   │   ├── metadata_client.h
│   │   └── node_client.h
│   └── utils/
│       ├── hash_utils.h
│       └── progress_reporter.h
│
└── CMakeLists.txt
```

