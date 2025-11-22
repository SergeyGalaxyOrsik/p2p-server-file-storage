# Архитектура Client

## 1. Назначение и ответственность

Client является консольной утилитой для взаимодействия пользователя с системой CourseStore. Он отвечает за:
- Загрузку файлов в систему (разбиение на чанки, репликация, отправка)
- Скачивание файлов из системы (получение чанков, сборка файла)
- Взаимодействие с Metadata Server и Storage Nodes
- Валидацию целостности файлов

## 2. Структура компонентов

### 2.1. Основные модули

```
client/
├── src/
│   ├── main.cpp                 # Точка входа, парсинг команд
│   ├── client.cpp               # Основной класс клиента
│   ├── upload_manager.cpp       # Управление загрузкой
│   ├── download_manager.cpp     # Управление скачиванием
│   ├── chunk_processor.cpp      # Разбиение и сборка файлов
│   ├── metadata_client.cpp      # Взаимодействие с Metadata Server
│   ├── node_client.cpp          # Взаимодействие с Storage Nodes
│   └── hash_utils.cpp           # Вычисление хешей (SHA-256)
├── include/
│   ├── client.h
│   ├── upload_manager.h
│   ├── download_manager.h
│   ├── chunk_processor.h
│   ├── metadata_client.h
│   ├── node_client.h
│   ├── hash_utils.h
│   └── protocol.h
└── CMakeLists.txt
```

## 3. Классовая структура

### 3.1. Класс `Client`

**Ответственность:** Основной класс клиента, обработка команд

```cpp
class Client {
private:
    std::string metadataServerIp;
    int metadataServerPort;
    MetadataClient metadataClient;
    UploadManager uploadManager;
    DownloadManager downloadManager;
    
public:
    bool Initialize(const std::string& metadataServerIp, int port);
    bool ExecuteCommand(const std::vector<std::string>& args);
    void Shutdown();
};
```

### 3.2. Класс `ChunkProcessor`

**Ответственность:** Разбиение файлов на чанки и сборка обратно

```cpp
struct Chunk {
    std::string chunkId;          // SHA-256 хеш
    size_t index;                 // Порядковый номер
    size_t size;                  // Размер чанка
    std::vector<uint8_t> data;    // Данные чанка
};

class ChunkProcessor {
private:
    static const size_t CHUNK_SIZE = 1048576;  // 1 МБ
    
public:
    static std::vector<Chunk> SplitFile(const std::string& filepath);
    static bool AssembleFile(const std::vector<Chunk>& chunks,
                            const std::string& outputPath);
    static std::string CalculateChunkHash(const std::vector<uint8_t>& data);
    static bool ValidateFile(const std::string& filepath,
                            const std::string& expectedHash);
};
```

**Реализация разбиения файла:**

```cpp
std::vector<Chunk> ChunkProcessor::SplitFile(const std::string& filepath) {
    std::vector<Chunk> chunks;
    
    HANDLE hFile = CreateFileA(
        filepath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        return chunks;
    }
    
    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);
    
    size_t index = 0;
    std::vector<uint8_t> buffer(CHUNK_SIZE);
    
    while (true) {
        DWORD bytesRead;
        if (!ReadFile(hFile, buffer.data(), CHUNK_SIZE, &bytesRead, NULL)) {
            break;
        }
        
        if (bytesRead == 0) {
            break;
        }
        
        buffer.resize(bytesRead);
        
        Chunk chunk;
        chunk.index = index;
        chunk.size = bytesRead;
        chunk.data = buffer;
        chunk.chunkId = CalculateChunkHash(buffer);
        
        chunks.push_back(chunk);
        index++;
        
        buffer.resize(CHUNK_SIZE);
    }
    
    CloseHandle(hFile);
    return chunks;
}
```

**Реализация сборки файла:**

```cpp
bool ChunkProcessor::AssembleFile(const std::vector<Chunk>& chunks,
                                  const std::string& outputPath) {
    // Сортировка чанков по индексу
    std::vector<Chunk> sortedChunks = chunks;
    std::sort(sortedChunks.begin(), sortedChunks.end(),
              [](const Chunk& a, const Chunk& b) {
                  return a.index < b.index;
              });
    
    HANDLE hFile = CreateFileA(
        outputPath.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    for (const auto& chunk : sortedChunks) {
        DWORD bytesWritten;
        if (!WriteFile(hFile, chunk.data.data(),
                      static_cast<DWORD>(chunk.data.size()),
                      &bytesWritten, NULL)) {
            CloseHandle(hFile);
            return false;
        }
    }
    
    CloseHandle(hFile);
    return true;
}
```

### 3.3. Класс `UploadManager`

**Ответственность:** Управление процессом загрузки файла

```cpp
class UploadManager {
private:
    MetadataClient* metadataClient;
    NodeClient nodeClient;
    ChunkProcessor chunkProcessor;
    
public:
    UploadManager(MetadataClient* metadataClient);
    bool UploadFile(const std::string& localPath, 
                   const std::string& remoteFilename);
    
private:
    bool UploadChunk(const Chunk& chunk, 
                    const std::vector<StorageNode>& nodes);
    bool NotifyUploadComplete(const std::string& filename,
                             const std::vector<Chunk>& chunks);
};
```

**Алгоритм загрузки:**

```cpp
bool UploadManager::UploadFile(const std::string& localPath,
                              const std::string& remoteFilename) {
    // 1. Разбиение файла на чанки
    std::vector<Chunk> chunks = chunkProcessor.SplitFile(localPath);
    if (chunks.empty()) {
        return false;
    }
    
    // 2. Запрос узлов у Metadata Server
    uint64_t totalSize = 0;
    for (const auto& chunk : chunks) {
        totalSize += chunk.size;
    }
    
    std::vector<StorageNode> nodes = metadataClient->RequestUploadNodes(
        remoteFilename, totalSize, chunks.size() * 2);  // *2 для репликации
    
    if (nodes.size() < chunks.size() * 2) {
        std::cerr << "Not enough storage nodes available" << std::endl;
        return false;
    }
    
    // 3. Загрузка каждого чанка с репликацией
    size_t nodeIndex = 0;
    for (const auto& chunk : chunks) {
        // Выбор 2 узлов для репликации
        std::vector<StorageNode> chunkNodes;
        for (int i = 0; i < 2 && nodeIndex < nodes.size(); i++) {
            chunkNodes.push_back(nodes[nodeIndex++]);
        }
        
        if (!UploadChunk(chunk, chunkNodes)) {
            std::cerr << "Failed to upload chunk " << chunk.chunkId << std::endl;
            return false;
        }
    }
    
    // 4. Уведомление Metadata Server о завершении
    return NotifyUploadComplete(remoteFilename, chunks);
}
```

### 3.4. Класс `DownloadManager`

**Ответственность:** Управление процессом скачивания файла

```cpp
class DownloadManager {
private:
    MetadataClient* metadataClient;
    NodeClient nodeClient;
    ChunkProcessor chunkProcessor;
    
public:
    DownloadManager(MetadataClient* metadataClient);
    bool DownloadFile(const std::string& remoteFilename,
                    const std::string& localPath);
    
private:
    bool DownloadChunk(const std::string& chunkId,
                      const std::vector<std::string>& nodeIds,
                      Chunk& chunk);
    Chunk DownloadChunkFromNode(const std::string& chunkId,
                               const std::string& nodeIp, int nodePort);
};
```

**Алгоритм скачивания:**

```cpp
bool DownloadManager::DownloadFile(const std::string& remoteFilename,
                                  const std::string& localPath) {
    // 1. Запрос метаданных файла
    FileMetadata metadata = metadataClient->RequestDownload(remoteFilename);
    if (metadata.filename.empty()) {
        std::cerr << "File not found" << std::endl;
        return false;
    }
    
    // 2. Параллельное скачивание чанков
    std::vector<Chunk> chunks;
    std::mutex chunksMutex;
    std::vector<std::thread> threads;
    
    for (const auto& chunkInfo : metadata.chunks) {
        threads.emplace_back([&, chunkInfo]() {
            Chunk chunk;
            if (DownloadChunk(chunkInfo.chunkId, chunkInfo.nodeIds, chunk)) {
                std::lock_guard<std::mutex> lock(chunksMutex);
                chunks.push_back(chunk);
            }
        });
    }
    
    // Ожидание завершения всех потоков
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Проверка, что все чанки скачаны
    if (chunks.size() != metadata.chunks.size()) {
        std::cerr << "Failed to download all chunks" << std::endl;
        return false;
    }
    
    // 3. Сборка файла
    return chunkProcessor.AssembleFile(chunks, localPath);
}
```

**Скачивание с отказоустойчивостью:**

```cpp
bool DownloadManager::DownloadChunk(const std::string& chunkId,
                                   const std::vector<std::string>& nodeIds,
                                   Chunk& chunk) {
    // Попытка скачать с первого доступного узла
    for (const auto& nodeId : nodeIds) {
        // Получение информации об узле из метаданных
        StorageNode node = GetNodeInfo(nodeId);
        
        Chunk downloadedChunk = DownloadChunkFromNode(
            chunkId, node.ipAddress, node.port);
        
        if (!downloadedChunk.data.empty()) {
            chunk = downloadedChunk;
            return true;
        }
    }
    
    return false;
}
```

### 3.5. Класс `MetadataClient`

**Ответственность:** Взаимодействие с Metadata Server

```cpp
class MetadataClient {
private:
    std::string serverIp;
    int serverPort;
    
public:
    bool Connect(const std::string& ip, int port);
    std::vector<StorageNode> RequestUploadNodes(
        const std::string& filename, uint64_t size, size_t nodeCount);
    bool NotifyUploadComplete(const std::string& filename,
                             const std::vector<ChunkInfo>& chunks);
    FileMetadata RequestDownload(const std::string& filename);
    std::vector<std::string> ListFiles();
    void Disconnect();
};
```

### 3.6. Класс `NodeClient`

**Ответственность:** Взаимодействие с Storage Nodes

```cpp
class NodeClient {
public:
    bool StoreChunk(const std::string& nodeIp, int nodePort,
                   const std::string& chunkId,
                   const std::vector<uint8_t>& data);
    bool GetChunk(const std::string& nodeIp, int nodePort,
                 const std::string& chunkId,
                 std::vector<uint8_t>& data);
    bool CheckChunk(const std::string& nodeIp, int nodePort,
                   const std::string& chunkId);
};
```

## 4. Интерфейс командной строки

### 4.1. Формат команд

```
coursestore-client.exe <command> [arguments]
```

### 4.2. Поддерживаемые команды

- `upload <local_path> <remote_filename>` - Загрузка файла
- `download <remote_filename> <local_path>` - Скачивание файла
- `list` - Список файлов в системе
- `help` - Справка

### 4.3. Пример использования

```cpp
int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintUsage();
        return 1;
    }
    
    Client client;
    if (!client.Initialize("127.0.0.1", 8080)) {
        std::cerr << "Failed to initialize client" << std::endl;
        return 1;
    }
    
    std::vector<std::string> args(argv + 1, argv + argc);
    if (!client.ExecuteCommand(args)) {
        return 1;
    }
    
    return 0;
}
```

## 5. Обработка ошибок

- Файл не найден (локально) → сообщение об ошибке
- Недостаточно узлов → сообщение об ошибке
- Ошибка сети → повторные попытки (для скачивания)
- Неполное скачивание → сообщение об ошибке
- Несовпадение хешей → предупреждение

## 6. Валидация целостности

### 6.1. Проверка хешей чанков

При скачивании каждый чанк проверяется на соответствие его chunkId (SHA-256):

```cpp
bool ValidateChunk(const Chunk& chunk) {
    std::string calculatedHash = ChunkProcessor::CalculateChunkHash(chunk.data);
    return calculatedHash == chunk.chunkId;
}
```

### 6.2. Проверка целостности файла

Опционально: вычисление и сравнение MD5/SHA-256 всего файла после сборки.

## 7. Оптимизации

### 7.1. Параллельная загрузка чанков

Использование потоков для одновременной загрузки нескольких чанков.

### 7.2. Параллельное скачивание

Скачивание всех чанков параллельно для ускорения процесса.

### 7.3. Буферизация

Использование буферов при чтении/записи файлов для повышения производительности.

## 8. Логирование

- Прогресс загрузки/скачивания (процент завершения)
- Информация о подключении к узлам
- Ошибки с деталями

