# Структура проекта CourseStore

## 1. Общая структура монорепозитория

```
new-storage/
├── CMakeLists.txt                    # Корневой CMake файл
├── README.md                         # Основная документация
│
├── common/                           # Общий код для всех проектов
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── protocol.h                # Определения протокола
│   │   ├── network_utils.h            # Сетевые утилиты
│   │   ├── hash_utils.h               # Утилиты для хеширования
│   │   └── chunk_utils.h              # Утилиты для работы с чанками
│   └── src/
│       ├── protocol.cpp
│       ├── network_utils.cpp
│       ├── hash_utils.cpp
│       └── chunk_utils.cpp
│
├── metadata-server/                  # Проект сервера метаданных
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── server.h
│   │   ├── node_manager.h
│   │   ├── metadata_manager.h
│   │   └── protocol_handler.h
│   └── src/
│       ├── main.cpp
│       ├── server.cpp
│       ├── node_manager.cpp
│       ├── metadata_manager.cpp
│       └── protocol_handler.cpp
│
├── storage-node/                      # Проект узла хранения
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── node.h
│   │   ├── chunk_storage.h
│   │   ├── metadata_client.h
│   │   └── protocol_handler.h
│   └── src/
│       ├── main.cpp
│       ├── node.cpp
│       ├── chunk_storage.cpp
│       ├── metadata_client.cpp
│       ├── protocol_handler.cpp
│       └── disk_utils.cpp
│
├── client/                            # Проект клиента
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── client.h
│   │   ├── upload_manager.h
│   │   ├── download_manager.h
│   │   ├── chunk_processor.h
│   │   ├── metadata_client.h
│   │   └── node_client.h
│   └── src/
│       ├── main.cpp
│       ├── client.cpp
│       ├── upload_manager.cpp
│       ├── download_manager.cpp
│       ├── chunk_processor.cpp
│       ├── metadata_client.cpp
│       └── node_client.cpp
│
└── docs/                              # Документация
    └── architecture/
        ├── 01-overview.md
        ├── 02-metadata-server.md
        ├── 03-storage-node.md
        ├── 04-client.md
        ├── 05-network-protocol.md
        └── 06-project-structure.md
```

## 2. Детальная структура модулей

### 2.1. Common (Общий код)

#### protocol.h
```cpp
// Определения команд протокола
namespace Protocol {
    // Команды Metadata Server
    constexpr const char* REGISTER_NODE = "REGISTER_NODE";
    constexpr const char* KEEP_ALIVE = "KEEP_ALIVE";
    constexpr const char* REQUEST_UPLOAD = "REQUEST_UPLOAD";
    constexpr const char* UPLOAD_COMPLETE = "UPLOAD_COMPLETE";
    constexpr const char* REQUEST_DOWNLOAD = "REQUEST_DOWNLOAD";
    constexpr const char* LIST_FILES = "LIST_FILES";
    
    // Команды Storage Node
    constexpr const char* STORE_CHUNK = "STORE_CHUNK";
    constexpr const char* GET_CHUNK = "GET_CHUNK";
    constexpr const char* DELETE_CHUNK = "DELETE_CHUNK";
    constexpr const char* CHECK_CHUNK = "CHECK_CHUNK";
    
    // Структуры данных
    struct StorageNodeInfo {
        std::string nodeId;
        std::string ipAddress;
        int port;
        uint64_t freeSpace;
    };
    
    struct ChunkInfo {
        std::string chunkId;
        size_t index;
        size_t size;
        std::vector<std::string> nodeIds;
    };
}
```

#### network_utils.h
```cpp
namespace NetworkUtils {
    bool InitializeWinsock();
    void CleanupWinsock();
    bool SendMessage(SOCKET socket, const std::string& message);
    bool ReceiveMessage(SOCKET socket, std::string& message, size_t maxSize = 4096);
    bool SendBinaryData(SOCKET socket, const std::vector<uint8_t>& data);
    bool ReceiveBinaryData(SOCKET socket, std::vector<uint8_t>& data, size_t size);
    std::string GetLocalIPAddress();
}
```

#### hash_utils.h
```cpp
namespace HashUtils {
    std::string CalculateSHA256(const std::vector<uint8_t>& data);
    std::string CalculateSHA256(const std::string& filepath);
    bool VerifyHash(const std::vector<uint8_t>& data, const std::string& expectedHash);
}
```

#### chunk_utils.h
```cpp
namespace ChunkUtils {
    constexpr size_t CHUNK_SIZE = 1048576;  // 1 МБ
    
    struct Chunk {
        std::string chunkId;
        size_t index;
        size_t size;
        std::vector<uint8_t> data;
    };
    
    std::vector<Chunk> SplitFile(const std::string& filepath);
    bool AssembleFile(const std::vector<Chunk>& chunks, const std::string& outputPath);
}
```

### 2.2. Metadata Server

#### server.h
```cpp
class MetadataServer {
private:
    SOCKET listenSocket;
    int port;
    NodeManager nodeManager;
    MetadataManager metadataManager;
    ProtocolHandler protocolHandler;
    bool running;
    
public:
    MetadataServer(int port);
    ~MetadataServer();
    
    bool Initialize();
    void Run();
    void Shutdown();
    
private:
    void HandleClient(SOCKET clientSocket);
    static void ClientHandlerThread(MetadataServer* server, SOCKET clientSocket);
};
```

#### node_manager.h
```cpp
struct StorageNode {
    std::string nodeId;
    std::string ipAddress;
    int port;
    uint64_t freeSpace;
    std::chrono::time_point<std::chrono::steady_clock> lastSeen;
    bool isActive;
};

class NodeManager {
private:
    std::unordered_map<std::string, StorageNode> nodes;
    std::mutex nodesMutex;
    std::thread keepAliveThread;
    bool running;
    
public:
    NodeManager();
    ~NodeManager();
    
    bool RegisterNode(const std::string& ip, int port, 
                     uint64_t freeSpace, std::string& nodeId);
    bool UnregisterNode(const std::string& nodeId);
    std::vector<StorageNode> GetAvailableNodes(size_t count, 
                                               uint64_t requiredSpace);
    void UpdateNodeLastSeen(const std::string& nodeId);
    void UpdateNodeSpace(const std::string& nodeId, uint64_t freeSpace);
    StorageNode* GetNode(const std::string& nodeId);
    void StartKeepAliveChecker();
    void StopKeepAliveChecker();
    
private:
    void CheckNodeHealth();
    std::string GenerateNodeId();
};
```

#### metadata_manager.h
```cpp
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
    std::vector<FileMetadata> GetAllFiles();
    
    // Опционально: сохранение/загрузка
    void SaveToFile(const std::string& path);
    bool LoadFromFile(const std::string& path);
};
```

#### protocol_handler.h
```cpp
class ProtocolHandler {
private:
    NodeManager* nodeManager;
    MetadataManager* metadataManager;
    
public:
    ProtocolHandler(NodeManager* nodeManager, MetadataManager* metadataManager);
    
    std::string ProcessRequest(const std::string& request);
    
private:
    std::string HandleRegisterNode(const std::vector<std::string>& args);
    std::string HandleKeepAlive(const std::vector<std::string>& args);
    std::string HandleUpdateSpace(const std::vector<std::string>& args);
    std::string HandleRequestUpload(const std::vector<std::string>& args);
    std::string HandleUploadComplete(const std::string& request);
    std::string HandleRequestDownload(const std::vector<std::string>& args);
    std::string HandleListFiles();
    std::vector<std::string> ParseCommand(const std::string& command);
};
```

### 2.3. Storage Node

#### node.h
```cpp
class StorageNode {
private:
    std::string nodeId;
    std::string storagePath;
    std::string metadataServerIp;
    int metadataServerPort;
    int listenPort;
    SOCKET listenSocket;
    ChunkStorage chunkStorage;
    MetadataClient metadataClient;
    ProtocolHandler protocolHandler;
    std::thread keepAliveThread;
    bool running;
    
public:
    StorageNode(const std::string& storagePath,
               const std::string& metadataServerIp,
               int metadataServerPort);
    ~StorageNode();
    
    bool Initialize();
    bool RegisterWithMetadataServer();
    void Run();
    void Shutdown();
    
private:
    void HandleClient(SOCKET clientSocket);
    void StartKeepAliveThread();
    void KeepAliveLoop();
    static void ClientHandlerThread(StorageNode* node, SOCKET clientSocket);
};
```

#### chunk_storage.h
```cpp
class ChunkStorage {
private:
    std::string storagePath;
    std::mutex storageMutex;
    std::unordered_map<std::string, std::string> chunkMap;  // chunkId -> filepath
    
public:
    ChunkStorage(const std::string& path);
    
    bool Initialize();
    bool StoreChunk(const std::string& chunkId, 
                   const std::vector<uint8_t>& data);
    bool GetChunk(const std::string& chunkId, 
                 std::vector<uint8_t>& data);
    bool DeleteChunk(const std::string& chunkId);
    bool ChunkExists(const std::string& chunkId);
    uint64_t GetTotalSize();
    uint64_t GetFreeSpace();
    std::vector<std::string> ListChunks();
    
private:
    std::string GetChunkPath(const std::string& chunkId);
};
```

#### metadata_client.h
```cpp
class MetadataClient {
private:
    std::string serverIp;
    int serverPort;
    std::string nodeId;
    
public:
    MetadataClient(const std::string& ip, int port);
    
    bool RegisterNode(const std::string& localIp, int localPort,
                     uint64_t freeSpace, std::string& outNodeId);
    bool SendKeepAlive(const std::string& nodeId);
    bool UpdateFreeSpace(const std::string& nodeId, uint64_t freeSpace);
    
private:
    bool ConnectToServer(SOCKET& socket);
    bool SendRequest(SOCKET socket, const std::string& request);
    bool ReceiveResponse(SOCKET socket, std::string& response);
};
```

#### protocol_handler.h
```cpp
class ProtocolHandler {
private:
    ChunkStorage* chunkStorage;
    
public:
    ProtocolHandler(ChunkStorage* chunkStorage);
    
    std::string ProcessRequest(const std::string& request, SOCKET clientSocket);
    
private:
    std::string HandleStoreChunk(const std::vector<std::string>& args, SOCKET socket);
    std::string HandleGetChunk(const std::vector<std::string>& args, SOCKET socket);
    std::string HandleDeleteChunk(const std::vector<std::string>& args);
    std::string HandleCheckChunk(const std::vector<std::string>& args);
    std::vector<std::string> ParseCommand(const std::string& command);
};
```

#### disk_utils.h
```cpp
class DiskUtils {
public:
    static uint64_t GetFreeSpace(const std::string& path);
    static uint64_t GetTotalSpace(const std::string& path);
    static bool CreateDirectoryIfNotExists(const std::string& path);
    static bool DirectoryExists(const std::string& path);
};
```

### 2.4. Client

#### client.h
```cpp
class Client {
private:
    std::string metadataServerIp;
    int metadataServerPort;
    MetadataClient metadataClient;
    UploadManager uploadManager;
    DownloadManager downloadManager;
    
public:
    Client(const std::string& metadataServerIp, int port);
    
    bool Initialize();
    bool ExecuteCommand(const std::vector<std::string>& args);
    void Shutdown();
    
private:
    void PrintUsage();
    bool HandleUpload(const std::vector<std::string>& args);
    bool HandleDownload(const std::vector<std::string>& args);
    bool HandleList();
};
```

#### upload_manager.h
```cpp
class UploadManager {
private:
    MetadataClient* metadataClient;
    NodeClient nodeClient;
    
public:
    UploadManager(MetadataClient* metadataClient);
    
    bool UploadFile(const std::string& localPath, 
                   const std::string& remoteFilename);
    
private:
    bool UploadChunk(const Chunk& chunk, 
                    const std::vector<StorageNodeInfo>& nodes);
    bool NotifyUploadComplete(const std::string& filename,
                             const std::vector<ChunkInfo>& chunks);
};
```

#### download_manager.h
```cpp
class DownloadManager {
private:
    MetadataClient* metadataClient;
    NodeClient nodeClient;
    
public:
    DownloadManager(MetadataClient* metadataClient);
    
    bool DownloadFile(const std::string& remoteFilename,
                    const std::string& localPath);
    
private:
    bool DownloadChunk(const ChunkInfo& chunkInfo, Chunk& chunk);
    Chunk DownloadChunkFromNode(const std::string& chunkId,
                               const StorageNodeInfo& node);
};
```

#### chunk_processor.h
```cpp
class ChunkProcessor {
public:
    static std::vector<Chunk> SplitFile(const std::string& filepath);
    static bool AssembleFile(const std::vector<Chunk>& chunks,
                            const std::string& outputPath);
    static std::string CalculateChunkHash(const std::vector<uint8_t>& data);
    static bool ValidateChunk(const Chunk& chunk);
};
```

#### metadata_client.h
```cpp
class MetadataClient {
private:
    std::string serverIp;
    int serverPort;
    
public:
    MetadataClient(const std::string& ip, int port);
    
    bool Connect();
    std::vector<StorageNodeInfo> RequestUploadNodes(
        const std::string& filename, uint64_t size, size_t nodeCount);
    bool NotifyUploadComplete(const std::string& filename,
                             const std::vector<ChunkInfo>& chunks);
    FileMetadata RequestDownload(const std::string& filename);
    std::vector<std::string> ListFiles();
    void Disconnect();
    
private:
    bool SendRequest(const std::string& request);
    bool ReceiveResponse(std::string& response);
    SOCKET currentSocket;
};
```

#### node_client.h
```cpp
class NodeClient {
public:
    bool StoreChunk(const StorageNodeInfo& node,
                   const std::string& chunkId,
                   const std::vector<uint8_t>& data);
    bool GetChunk(const StorageNodeInfo& node,
                 const std::string& chunkId,
                 std::vector<uint8_t>& data);
    bool CheckChunk(const StorageNodeInfo& node,
                   const std::string& chunkId);
    
private:
    bool ConnectToNode(const StorageNodeInfo& node, SOCKET& socket);
};
```

## 3. CMake структура

### 3.1. Корневой CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.15)
project(CourseStore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Общие настройки
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7+
endif()

# Подпроекты
add_subdirectory(common)
add_subdirectory(metadata-server)
add_subdirectory(storage-node)
add_subdirectory(client)
```

### 3.2. Common CMakeLists.txt

```cmake
add_library(common STATIC
    src/protocol.cpp
    src/network_utils.cpp
    src/hash_utils.cpp
    src/chunk_utils.cpp
)

target_include_directories(common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(WIN32)
    target_link_libraries(common ws2_32)
    target_link_libraries(common crypt32)  # Для CryptoAPI (SHA-256)
endif()
```

### 3.3. Metadata Server CMakeLists.txt

```cmake
add_executable(metadata-server
    src/main.cpp
    src/server.cpp
    src/node_manager.cpp
    src/metadata_manager.cpp
    src/protocol_handler.cpp
)

target_include_directories(metadata-server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(metadata-server common)

if(WIN32)
    target_link_libraries(metadata-server ws2_32)
endif()
```

### 3.4. Storage Node CMakeLists.txt

```cmake
add_executable(storage-node
    src/main.cpp
    src/node.cpp
    src/chunk_storage.cpp
    src/metadata_client.cpp
    src/protocol_handler.cpp
    src/disk_utils.cpp
)

target_include_directories(storage-node PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(storage-node common)

if(WIN32)
    target_link_libraries(storage-node ws2_32)
endif()
```

### 3.5. Client CMakeLists.txt

```cmake
add_executable(client
    src/main.cpp
    src/client.cpp
    src/upload_manager.cpp
    src/download_manager.cpp
    src/chunk_processor.cpp
    src/metadata_client.cpp
    src/node_client.cpp
)

target_include_directories(client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(client common)

if(WIN32)
    target_link_libraries(client ws2_32)
endif()
```

## 4. Зависимости

### 4.1. Системные библиотеки

- **Winsock 2** (`ws2_32.lib`) - Сетевое программирование
- **Windows API** - Системные функции
- **CryptoAPI** (`crypt32.lib`) - Для SHA-256 (опционально, можно использовать библиотеку)

### 4.2. Внешние библиотеки (опционально)

- **OpenSSL** - Для SHA-256 (если не использовать CryptoAPI)
- **JSON библиотека** - Для сохранения метаданных (например, nlohmann/json)

## 5. Сборка проекта

### 5.1. Требования

- CMake 3.15+
- Visual Studio 2019+ или MinGW-w64
- Windows SDK

### 5.2. Команды сборки

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### 5.3. Результаты сборки

```
build/
├── Release/
│   ├── metadata-server.exe
│   ├── storage-node.exe
│   └── client.exe
```

## 6. Конфигурация

### 6.1. Параметры Metadata Server

- Порт по умолчанию: 8080
- Таймаут узла: 60 секунд
- Интервал keep-alive проверки: 30 секунд

### 6.2. Параметры Storage Node

- Путь хранения: `./storage/`
- Размер чанка: 1 МБ
- Интервал keep-alive: 30 секунд

### 6.3. Параметры Client

- Адрес Metadata Server: `127.0.0.1:8080`
- Коэффициент репликации: 2

