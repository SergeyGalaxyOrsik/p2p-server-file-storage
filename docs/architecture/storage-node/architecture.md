# Детальная архитектура Storage Node

## 1. Обзор архитектуры

Storage Node построен по принципу разделения на **ядро (core)** и **платформо-зависимые обработчики (platform handlers)**. Это позволяет:

- Изолировать платформо-зависимый код
- Легко добавлять поддержку новых ОС
- Упростить тестирование (можно мокировать обработчики)
- Обеспечить кроссплатформенность

### 1.1. Структура компонентов

```
Storage Node
├── Core (платформо-независимый код)
│   ├── StorageNode (главный класс)
│   ├── ChunkStorage (управление чанками)
│   ├── MetadataClient (взаимодействие с Metadata Server)
│   └── ProtocolHandler (обработка протокола)
│
└── Platform Handlers (платформо-зависимый код)
    ├── Interfaces (абстракции)
    │   ├── IFileHandler (работа с файлами)
    │   ├── IDiskHandler (работа с диском)
    │   └── IPathHandler (работа с путями)
    │
    └── Implementations (реализации)
        ├── Windows
        │   ├── WindowsFileHandler
        │   ├── WindowsDiskHandler
        │   └── WindowsPathHandler
        │
        └── Linux
            ├── LinuxFileHandler
            ├── LinuxDiskHandler
            └── LinuxPathHandler
```

## 2. Ядро (Core)

### 2.1. Класс StorageNode

**Назначение:** Главный класс узла, координирующий работу всех компонентов.

**Полная сигнатура:**

```cpp
class StorageNode {
private:
    // Идентификация
    std::string nodeId;
    std::string storagePath;
    
    // Конфигурация Metadata Server
    std::string metadataServerIp;
    int metadataServerPort;
    
    // Сетевые компоненты
    int listenPort;
    SOCKET listenSocket;
    bool running;
    
    // Компоненты ядра
    std::unique_ptr<ChunkStorage> chunkStorage;
    std::unique_ptr<MetadataClient> metadataClient;
    std::unique_ptr<ProtocolHandler> protocolHandler;
    
    // Потоки
    std::thread keepAliveThread;
    std::vector<std::thread> clientThreads;
    
    // Конфигурация
    static const int KEEP_ALIVE_INTERVAL_SEC = 30;
    static const int MAX_CLIENTS = 100;
    static const int SOCKET_TIMEOUT_SEC = 30;
    
public:
    StorageNode(const std::string& storagePath,
               const std::string& metadataServerIp,
               int metadataServerPort);
    ~StorageNode();
    
    // Инициализация и запуск
    bool Initialize();
    bool RegisterWithMetadataServer();
    void Run();
    void Shutdown();
    
    // Обработка клиентов
    void HandleClient(SOCKET clientSocket);
    static void ClientHandlerThread(StorageNode* node, SOCKET clientSocket);
    
    // Keep-Alive
    void StartKeepAliveThread();
    void StopKeepAliveThread();
    void KeepAliveLoop();
    
    // Геттеры
    const std::string& GetNodeId() const { return nodeId; }
    const std::string& GetStoragePath() const { return storagePath; }
    int GetListenPort() const { return listenPort; }
    
private:
    // Внутренние методы
    bool InitializeNetwork();
    bool CreateListenSocket();
    void AcceptLoop();
    void Cleanup();
    std::string GetLocalIPAddress();
};
```

**Жизненный цикл:**

1. **Инициализация:**
   - Создание платформо-зависимых обработчиков
   - Инициализация ChunkStorage с обработчиками
   - Инициализация сетевого слоя
   - Создание слушающего сокета

2. **Регистрация:**
   - Получение свободного места через IDiskHandler
   - Регистрация на Metadata Server
   - Сохранение nodeId

3. **Работа:**
   - Основной цикл приёма соединений
   - Обработка клиентов в отдельных потоках
   - Keep-alive поток

4. **Завершение:**
   - Корректное закрытие соединений
   - Остановка потоков
   - Очистка ресурсов

### 2.2. Класс ChunkStorage

**Назначение:** Управление хранением чанков на диске. Использует платформо-зависимые обработчики через интерфейсы.

**Полная сигнатура:**

```cpp
class ChunkStorage {
private:
    std::string storagePath;
    std::mutex storageMutex;
    
    // Платформо-зависимые обработчики (инъекция зависимостей)
    std::unique_ptr<IFileHandler> fileHandler;
    std::unique_ptr<IDiskHandler> diskHandler;
    std::unique_ptr<IPathHandler> pathHandler;
    
    // Кэш информации о чанках
    std::unordered_map<std::string, ChunkInfo> chunkMap;  // chunkId -> ChunkInfo
    
    struct ChunkInfo {
        std::string filepath;
        size_t size;
        std::chrono::time_point<std::chrono::steady_clock> storedAt;
    };
    
public:
    // Конструктор с инъекцией зависимостей
    ChunkStorage(std::unique_ptr<IFileHandler> fileHandler,
                std::unique_ptr<IDiskHandler> diskHandler,
                std::unique_ptr<IPathHandler> pathHandler);
    
    // Инициализация
    bool Initialize(const std::string& path);
    
    // Работа с чанками
    bool StoreChunk(const std::string& chunkId, 
                   const std::vector<uint8_t>& data);
    bool GetChunk(const std::string& chunkId, 
                 std::vector<uint8_t>& data);
    bool DeleteChunk(const std::string& chunkId);
    bool ChunkExists(const std::string& chunkId);
    
    // Информация
    uint64_t GetTotalSize();
    uint64_t GetFreeSpace();
    size_t GetChunkCount();
    std::vector<std::string> ListChunks();
    
    // Валидация
    bool ValidateChunkId(const std::string& chunkId);
    
private:
    // Внутренние методы
    std::string GetChunkFilePath(const std::string& chunkId);
    bool EnsureStorageDirectory();
    void UpdateChunkInfo(const std::string& chunkId, size_t size);
    void RemoveChunkInfo(const std::string& chunkId);
};
```

**Реализация методов (платформо-независимая):**

```cpp
bool ChunkStorage::StoreChunk(const std::string& chunkId,
                             const std::vector<uint8_t>& data) {
    // Валидация
    if (!ValidateChunkId(chunkId) || data.empty()) {
        return false;
    }
    
    // Проверка свободного места
    uint64_t freeSpace = diskHandler->GetFreeSpace(storagePath);
    if (freeSpace < data.size()) {
        return false;  // Недостаточно места
    }
    
    // Получение пути к файлу
    std::string filepath = GetChunkFilePath(chunkId);
    
    // Сохранение через платформо-зависимый обработчик
    std::lock_guard<std::mutex> lock(storageMutex);
    
    if (!fileHandler->WriteFile(filepath, data)) {
        return false;
    }
    
    // Обновление кэша
    UpdateChunkInfo(chunkId, data.size());
    
    return true;
}

bool ChunkStorage::GetChunk(const std::string& chunkId,
                            std::vector<uint8_t>& data) {
    if (!ValidateChunkId(chunkId)) {
        return false;
    }
    
    std::string filepath = GetChunkFilePath(chunkId);
    
    std::lock_guard<std::mutex> lock(storageMutex);
    
    // Проверка существования
    if (!fileHandler->FileExists(filepath)) {
        return false;
    }
    
    // Чтение через платформо-зависимый обработчик
    return fileHandler->ReadFile(filepath, data);
}

std::string ChunkStorage::GetChunkFilePath(const std::string& chunkId) {
    std::string filename = chunkId + ".chunk";
    return pathHandler->Join(storagePath, filename);
}
```

### 2.3. Класс MetadataClient

**Назначение:** Взаимодействие с Metadata Server. Платформо-независимый (использует общий сетевой слой).

**Полная сигнатура:**

```cpp
class MetadataClient {
private:
    std::string serverIp;
    int serverPort;
    std::string nodeId;
    
    // Сетевые утилиты (из common)
    // Использует NetworkUtils из common библиотеки
    
public:
    MetadataClient(const std::string& ip, int port);
    ~MetadataClient();
    
    // Регистрация
    bool RegisterNode(const std::string& localIp, int localPort,
                     uint64_t freeSpace, std::string& outNodeId);
    
    // Keep-Alive
    bool SendKeepAlive(const std::string& nodeId);
    bool UpdateFreeSpace(const std::string& nodeId, uint64_t freeSpace);
    
    // Утилиты
    void SetNodeId(const std::string& id) { nodeId = id; }
    const std::string& GetNodeId() const { return nodeId; }
    
private:
    // Внутренние методы
    bool ConnectToServer(SOCKET& socket);
    bool SendRequest(SOCKET socket, const std::string& request);
    bool ReceiveResponse(SOCKET socket, std::string& response);
    bool ParseRegisterResponse(const std::string& response, std::string& nodeId);
};
```

**Реализация регистрации:**

```cpp
bool MetadataClient::RegisterNode(const std::string& localIp, int localPort,
                                 uint64_t freeSpace, std::string& outNodeId) {
    SOCKET sock = INVALID_SOCKET;
    
    if (!ConnectToServer(sock)) {
        return false;
    }
    
    // Формирование запроса
    std::string request = "REGISTER_NODE " + localIp + " " + 
                         std::to_string(localPort) + " " + 
                         std::to_string(freeSpace) + "\r\n";
    
    // Отправка запроса
    if (!NetworkUtils::SendMessage(sock, request)) {
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
    
    // Парсинг ответа
    if (!ParseRegisterResponse(response, outNodeId)) {
        return false;
    }
    
    nodeId = outNodeId;
    return true;
}

bool MetadataClient::ParseRegisterResponse(const std::string& response,
                                          std::string& nodeId) {
    // Формат: "REGISTER_RESPONSE OK <node_id>" или "REGISTER_RESPONSE ERROR <message>"
    std::vector<std::string> parts = SplitString(response, ' ');
    
    if (parts.size() < 3) {
        return false;
    }
    
    if (parts[0] == "REGISTER_RESPONSE" && parts[1] == "OK") {
        nodeId = parts[2];
        return true;
    }
    
    return false;
}
```

### 2.4. Класс ProtocolHandler

**Назначение:** Обработка протокола взаимодействия с клиентами.

**Полная сигнатура:**

```cpp
class ProtocolHandler {
private:
    ChunkStorage* chunkStorage;  // Указатель на хранилище
    
public:
    ProtocolHandler(ChunkStorage* chunkStorage);
    
    // Основной метод обработки
    std::string ProcessRequest(const std::string& request, SOCKET socket);
    
private:
    // Обработчики команд
    std::string HandleStoreChunk(const std::vector<std::string>& args, SOCKET socket);
    std::string HandleGetChunk(const std::vector<std::string>& args, SOCKET socket);
    std::string HandleDeleteChunk(const std::vector<std::string>& args);
    std::string HandleCheckChunk(const std::vector<std::string>& args);
    
    // Утилиты
    std::vector<std::string> ParseCommand(const std::string& command);
    std::string CreateErrorResponse(const std::string& errorCode, 
                                   const std::string& message);
    std::string CreateSuccessResponse(const std::string& data);
    bool ReceiveBinaryData(SOCKET socket, std::vector<uint8_t>& data, size_t size);
    bool SendBinaryData(SOCKET socket, const std::vector<uint8_t>& data);
};
```

**Обработка STORE_CHUNK:**

```cpp
std::string ProtocolHandler::HandleStoreChunk(
    const std::vector<std::string>& args, SOCKET socket) {
    
    // Валидация аргументов
    if (args.size() != 3) {
        return CreateErrorResponse("INVALID_PARAMETERS",
            "Expected: STORE_CHUNK <chunk_id> <size>");
    }
    
    std::string chunkId = args[1];
    size_t size = std::stoull(args[2]);
    
    // Валидация chunk_id (SHA-256 = 64 символа hex)
    if (chunkId.length() != 64) {
        return CreateErrorResponse("INVALID_CHUNK_ID",
            "Chunk ID must be 64 characters (SHA-256)");
    }
    
    // Получение бинарных данных
    std::vector<uint8_t> data;
    if (!ReceiveBinaryData(socket, data, size)) {
        return CreateErrorResponse("READ_ERROR",
            "Failed to receive chunk data");
    }
    
    // Сохранение чанка
    if (!chunkStorage->StoreChunk(chunkId, data)) {
        return CreateErrorResponse("STORE_ERROR",
            "Failed to store chunk");
    }
    
    return "STORE_RESPONSE OK\r\n";
}
```

## 3. Платформо-зависимые интерфейсы

### 3.1. Интерфейс IFileHandler

**Назначение:** Абстракция для работы с файлами.

```cpp
class IFileHandler {
public:
    virtual ~IFileHandler() = default;
    
    // Базовые операции
    virtual bool FileExists(const std::string& filepath) = 0;
    virtual bool ReadFile(const std::string& filepath, 
                         std::vector<uint8_t>& data) = 0;
    virtual bool WriteFile(const std::string& filepath,
                          const std::vector<uint8_t>& data) = 0;
    virtual bool DeleteFile(const std::string& filepath) = 0;
    
    // Информация о файле
    virtual bool GetFileSize(const std::string& filepath, uint64_t& size) = 0;
    virtual bool GetFileInfo(const std::string& filepath, FileInfo& info) = 0;
    
    // Утилиты
    virtual bool CreateDirectory(const std::string& path) = 0;
    virtual bool DirectoryExists(const std::string& path) = 0;
    
    struct FileInfo {
        uint64_t size;
        std::chrono::time_point<std::chrono::system_clock> lastModified;
        bool exists;
    };
};
```

### 3.2. Интерфейс IDiskHandler

**Назначение:** Абстракция для работы с дисковым пространством.

```cpp
class IDiskHandler {
public:
    virtual ~IDiskHandler() = default;
    
    // Информация о диске
    virtual uint64_t GetFreeSpace(const std::string& path) = 0;
    virtual uint64_t GetTotalSpace(const std::string& path) = 0;
    virtual uint64_t GetUsedSpace(const std::string& path) = 0;
    
    // Утилиты
    virtual bool IsPathValid(const std::string& path) = 0;
};
```

### 3.3. Интерфейс IPathHandler

**Назначение:** Абстракция для работы с путями файловой системы.

```cpp
class IPathHandler {
public:
    virtual ~IPathHandler() = default;
    
    // Операции с путями
    virtual std::string Join(const std::string& path1, 
                            const std::string& path2) = 0;
    virtual std::string GetDirectory(const std::string& filepath) = 0;
    virtual std::string GetFilename(const std::string& filepath) = 0;
    virtual std::string GetExtension(const std::string& filepath) = 0;
    
    // Нормализация
    virtual std::string Normalize(const std::string& path) = 0;
    virtual std::string Absolute(const std::string& path) = 0;
    
    // Разделители
    virtual char GetSeparator() const = 0;
    virtual std::string GetSeparatorString() const = 0;
};
```

## 4. Реализации для Windows

### 4.1. WindowsFileHandler

```cpp
class WindowsFileHandler : public IFileHandler {
public:
    WindowsFileHandler() = default;
    ~WindowsFileHandler() override = default;
    
    bool FileExists(const std::string& filepath) override {
        DWORD dwAttrib = GetFileAttributesA(filepath.c_str());
        return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
                !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
    }
    
    bool ReadFile(const std::string& filepath,
                 std::vector<uint8_t>& data) override {
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
            return false;
        }
        
        LARGE_INTEGER fileSize;
        if (!GetFileSizeEx(hFile, &fileSize)) {
            CloseHandle(hFile);
            return false;
        }
        
        data.resize(fileSize.QuadPart);
        
        DWORD bytesRead;
        bool success = ReadFile(
            hFile,
            data.data(),
            static_cast<DWORD>(data.size()),
            &bytesRead,
            NULL
        );
        
        CloseHandle(hFile);
        return success && (bytesRead == data.size());
    }
    
    bool WriteFile(const std::string& filepath,
                  const std::vector<uint8_t>& data) override {
        HANDLE hFile = CreateFileA(
            filepath.c_str(),
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
        
        DWORD bytesWritten;
        bool success = WriteFile(
            hFile,
            data.data(),
            static_cast<DWORD>(data.size()),
            &bytesWritten,
            NULL
        );
        
        CloseHandle(hFile);
        return success && (bytesWritten == data.size());
    }
    
    bool DeleteFile(const std::string& filepath) override {
        return ::DeleteFileA(filepath.c_str()) != 0;
    }
    
    bool GetFileSize(const std::string& filepath, uint64_t& size) override {
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
            return false;
        }
        
        LARGE_INTEGER fileSize;
        bool success = GetFileSizeEx(hFile, &fileSize);
        CloseHandle(hFile);
        
        if (success) {
            size = fileSize.QuadPart;
        }
        
        return success;
    }
    
    bool CreateDirectory(const std::string& path) override {
        return CreateDirectoryA(path.c_str(), NULL) != 0 ||
               GetLastError() == ERROR_ALREADY_EXISTS;
    }
    
    bool DirectoryExists(const std::string& path) override {
        DWORD dwAttrib = GetFileAttributesA(path.c_str());
        return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
                (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
    }
    
    bool GetFileInfo(const std::string& filepath, FileInfo& info) override {
        WIN32_FILE_ATTRIBUTE_DATA fileData;
        if (!GetFileAttributesExA(filepath.c_str(),
                                GetFileExInfoStandard,
                                &fileData)) {
            info.exists = false;
            return false;
        }
        
        info.exists = true;
        info.size = (static_cast<uint64_t>(fileData.nFileSizeHigh) << 32) |
                    fileData.nFileSizeLow;
        
        // Преобразование FILETIME в time_point
        ULARGE_INTEGER ul;
        ul.LowPart = fileData.ftLastWriteTime.dwLowDateTime;
        ul.HighPart = fileData.ftLastWriteTime.dwHighDateTime;
        
        auto time = std::chrono::system_clock::from_time_t(0);
        info.lastModified = time + std::chrono::seconds(ul.QuadPart / 10000000ULL - 11644473600ULL);
        
        return true;
    }
};
```

### 4.2. WindowsDiskHandler

```cpp
class WindowsDiskHandler : public IDiskHandler {
public:
    WindowsDiskHandler() = default;
    ~WindowsDiskHandler() override = default;
    
    uint64_t GetFreeSpace(const std::string& path) override {
        ULARGE_INTEGER freeBytesAvailable;
        ULARGE_INTEGER totalBytes;
        ULARGE_INTEGER totalFreeBytes;
        
        if (GetDiskFreeSpaceExA(
                path.c_str(),
                &freeBytesAvailable,
                &totalBytes,
                &totalFreeBytes)) {
            return freeBytesAvailable.QuadPart;
        }
        
        return 0;
    }
    
    uint64_t GetTotalSpace(const std::string& path) override {
        ULARGE_INTEGER freeBytesAvailable;
        ULARGE_INTEGER totalBytes;
        ULARGE_INTEGER totalFreeBytes;
        
        if (GetDiskFreeSpaceExA(
                path.c_str(),
                &freeBytesAvailable,
                &totalBytes,
                &totalFreeBytes)) {
            return totalBytes.QuadPart;
        }
        
        return 0;
    }
    
    uint64_t GetUsedSpace(const std::string& path) override {
        uint64_t total = GetTotalSpace(path);
        uint64_t free = GetFreeSpace(path);
        return total > free ? total - free : 0;
    }
    
    bool IsPathValid(const std::string& path) override {
        DWORD dwAttrib = GetFileAttributesA(path.c_str());
        return dwAttrib != INVALID_FILE_ATTRIBUTES;
    }
};
```

### 4.3. WindowsPathHandler

```cpp
class WindowsPathHandler : public IPathHandler {
public:
    WindowsPathHandler() = default;
    ~WindowsPathHandler() override = default;
    
    std::string Join(const std::string& path1,
                    const std::string& path2) override {
        if (path1.empty()) return path2;
        if (path2.empty()) return path1;
        
        if (path1.back() == '\\' || path1.back() == '/') {
            return path1 + path2;
        }
        
        return path1 + "\\" + path2;
    }
    
    std::string GetDirectory(const std::string& filepath) override {
        size_t pos = filepath.find_last_of("\\/");
        if (pos == std::string::npos) {
            return "";
        }
        return filepath.substr(0, pos);
    }
    
    std::string GetFilename(const std::string& filepath) override {
        size_t pos = filepath.find_last_of("\\/");
        if (pos == std::string::npos) {
            return filepath;
        }
        return filepath.substr(pos + 1);
    }
    
    std::string GetExtension(const std::string& filepath) override {
        std::string filename = GetFilename(filepath);
        size_t pos = filename.find_last_of('.');
        if (pos == std::string::npos) {
            return "";
        }
        return filename.substr(pos);
    }
    
    std::string Normalize(const std::string& path) override {
        std::string normalized = path;
        // Замена прямых слешей на обратные
        std::replace(normalized.begin(), normalized.end(), '/', '\\');
        // Удаление двойных разделителей
        // (упрощённая версия)
        return normalized;
    }
    
    std::string Absolute(const std::string& path) override {
        char buffer[MAX_PATH];
        if (GetFullPathNameA(path.c_str(), MAX_PATH, buffer, NULL)) {
            return std::string(buffer);
        }
        return path;
    }
    
    char GetSeparator() const override {
        return '\\';
    }
    
    std::string GetSeparatorString() const override {
        return "\\";
    }
};
```

## 5. Реализации для Linux

### 5.1. LinuxFileHandler

```cpp
class LinuxFileHandler : public IFileHandler {
public:
    LinuxFileHandler() = default;
    ~LinuxFileHandler() override = default;
    
    bool FileExists(const std::string& filepath) override {
        struct stat buffer;
        return (stat(filepath.c_str(), &buffer) == 0 && S_ISREG(buffer.st_mode));
    }
    
    bool ReadFile(const std::string& filepath,
                 std::vector<uint8_t>& data) override {
        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return false;
        }
        
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        data.resize(size);
        if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
            return false;
        }
        
        return true;
    }
    
    bool WriteFile(const std::string& filepath,
                  const std::vector<uint8_t>& data) override {
        std::ofstream file(filepath, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            return false;
        }
        
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        return file.good();
    }
    
    bool DeleteFile(const std::string& filepath) override {
        return unlink(filepath.c_str()) == 0;
    }
    
    bool GetFileSize(const std::string& filepath, uint64_t& size) override {
        struct stat buffer;
        if (stat(filepath.c_str(), &buffer) != 0) {
            return false;
        }
        
        size = buffer.st_size;
        return true;
    }
    
    bool CreateDirectory(const std::string& path) override {
        return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
    }
    
    bool DirectoryExists(const std::string& path) override {
        struct stat buffer;
        return (stat(path.c_str(), &buffer) == 0 && S_ISDIR(buffer.st_mode));
    }
    
    bool GetFileInfo(const std::string& filepath, FileInfo& info) override {
        struct stat buffer;
        if (stat(filepath.c_str(), &buffer) != 0) {
            info.exists = false;
            return false;
        }
        
        info.exists = true;
        info.size = buffer.st_size;
        info.lastModified = std::chrono::system_clock::from_time_t(buffer.st_mtime);
        
        return true;
    }
};
```

### 5.2. LinuxDiskHandler

```cpp
class LinuxDiskHandler : public IDiskHandler {
public:
    LinuxDiskHandler() = default;
    ~LinuxDiskHandler() override = default;
    
    uint64_t GetFreeSpace(const std::string& path) override {
        struct statvfs stat;
        if (statvfs(path.c_str(), &stat) != 0) {
            return 0;
        }
        
        return static_cast<uint64_t>(stat.f_bavail) * stat.f_frsize;
    }
    
    uint64_t GetTotalSpace(const std::string& path) override {
        struct statvfs stat;
        if (statvfs(path.c_str(), &stat) != 0) {
            return 0;
        }
        
        return static_cast<uint64_t>(stat.f_blocks) * stat.f_frsize;
    }
    
    uint64_t GetUsedSpace(const std::string& path) override {
        uint64_t total = GetTotalSpace(path);
        uint64_t free = GetFreeSpace(path);
        return total > free ? total - free : 0;
    }
    
    bool IsPathValid(const std::string& path) override {
        struct stat buffer;
        return stat(path.c_str(), &buffer) == 0;
    }
};
```

### 5.3. LinuxPathHandler

```cpp
class LinuxPathHandler : public IPathHandler {
public:
    LinuxPathHandler() = default;
    ~LinuxPathHandler() override = default;
    
    std::string Join(const std::string& path1,
                    const std::string& path2) override {
        if (path1.empty()) return path2;
        if (path2.empty()) return path1;
        
        if (path1.back() == '/') {
            return path1 + path2;
        }
        
        return path1 + "/" + path2;
    }
    
    std::string GetDirectory(const std::string& filepath) override {
        size_t pos = filepath.find_last_of('/');
        if (pos == std::string::npos) {
            return "";
        }
        return filepath.substr(0, pos);
    }
    
    std::string GetFilename(const std::string& filepath) override {
        size_t pos = filepath.find_last_of('/');
        if (pos == std::string::npos) {
            return filepath;
        }
        return filepath.substr(pos + 1);
    }
    
    std::string GetExtension(const std::string& filepath) override {
        std::string filename = GetFilename(filepath);
        size_t pos = filename.find_last_of('.');
        if (pos == std::string::npos) {
            return "";
        }
        return filename.substr(pos);
    }
    
    std::string Normalize(const std::string& path) override {
        // Упрощённая нормализация для Linux
        return path;
    }
    
    std::string Absolute(const std::string& path) override {
        char* resolved = realpath(path.c_str(), NULL);
        if (resolved) {
            std::string result(resolved);
            free(resolved);
            return result;
        }
        return path;
    }
    
    char GetSeparator() const override {
        return '/';
    }
    
    std::string GetSeparatorString() const override {
        return "/";
    }
};
```

## 6. Фабрика обработчиков

### 6.1. PlatformHandlerFactory

**Назначение:** Создание платформо-зависимых обработчиков.

```cpp
class PlatformHandlerFactory {
public:
    static std::unique_ptr<IFileHandler> CreateFileHandler() {
#ifdef _WIN32
        return std::make_unique<WindowsFileHandler>();
#elif __linux__
        return std::make_unique<LinuxFileHandler>();
#else
        #error "Unsupported platform"
#endif
    }
    
    static std::unique_ptr<IDiskHandler> CreateDiskHandler() {
#ifdef _WIN32
        return std::make_unique<WindowsDiskHandler>();
#elif __linux__
        return std::make_unique<LinuxDiskHandler>();
#else
        #error "Unsupported platform"
#endif
    }
    
    static std::unique_ptr<IPathHandler> CreatePathHandler() {
#ifdef _WIN32
        return std::make_unique<WindowsPathHandler>();
#elif __linux__
        return std::make_unique<LinuxPathHandler>();
#else
        #error "Unsupported platform"
#endif
    }
};
```

**Использование в StorageNode:**

```cpp
bool StorageNode::Initialize() {
    // Создание платформо-зависимых обработчиков
    auto fileHandler = PlatformHandlerFactory::CreateFileHandler();
    auto diskHandler = PlatformHandlerFactory::CreateDiskHandler();
    auto pathHandler = PlatformHandlerFactory::CreatePathHandler();
    
    // Создание ChunkStorage с обработчиками
    chunkStorage = std::make_unique<ChunkStorage>(
        std::move(fileHandler),
        std::move(diskHandler),
        std::move(pathHandler)
    );
    
    // Инициализация хранилища
    if (!chunkStorage->Initialize(storagePath)) {
        return false;
    }
    
    // Остальная инициализация...
    return true;
}
```

## 7. Структура проекта

```
storage-node/
├── src/
│   ├── core/
│   │   ├── node.cpp
│   │   ├── chunk_storage.cpp
│   │   ├── metadata_client.cpp
│   │   └── protocol_handler.cpp
│   │
│   ├── platform/
│   │   ├── interfaces/
│   │   │   ├── ifile_handler.h
│   │   │   ├── idisk_handler.h
│   │   │   └── ipath_handler.h
│   │   │
│   │   ├── windows/
│   │   │   ├── windows_file_handler.cpp
│   │   │   ├── windows_disk_handler.cpp
│   │   │   └── windows_path_handler.cpp
│   │   │
│   │   ├── linux/
│   │   │   ├── linux_file_handler.cpp
│   │   │   ├── linux_disk_handler.cpp
│   │   │   └── linux_path_handler.cpp
│   │   │
│   │   └── factory.cpp
│   │
│   └── main.cpp
│
├── include/
│   ├── core/
│   │   ├── node.h
│   │   ├── chunk_storage.h
│   │   ├── metadata_client.h
│   │   └── protocol_handler.h
│   │
│   └── platform/
│       ├── interfaces/
│       ├── windows/
│       ├── linux/
│       └── factory.h
│
└── CMakeLists.txt
```

## 8. Преимущества архитектуры

1. **Разделение ответственности:** Ядро не знает о платформе
2. **Тестируемость:** Можно легко мокировать обработчики
3. **Расширяемость:** Легко добавить поддержку новых ОС
4. **Поддерживаемость:** Платформо-зависимый код изолирован
5. **Кроссплатформенность:** Один код ядра для всех платформ

## 9. Обработка ошибок

Все обработчики должны:
- Возвращать понятные коды ошибок
- Логировать ошибки платформы
- Не раскрывать внутренние детали реализации
- Обеспечивать безопасность при ошибках

## 10. Тестирование

### 10.1. Моки для тестирования

```cpp
class MockFileHandler : public IFileHandler {
public:
    MOCK_METHOD(bool, FileExists, (const std::string&), (override));
    MOCK_METHOD(bool, ReadFile, (const std::string&, std::vector<uint8_t>&), (override));
    // ...
};
```

### 10.2. Unit-тесты ядра

- Тесты ChunkStorage с моками обработчиков
- Тесты без реальных файловых операций
- Быстрые и изолированные тесты

