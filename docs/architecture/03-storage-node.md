# Архитектура Storage Node

## 1. Назначение и ответственность

Storage Node предоставляет дисковое пространство для хранения чанков файлов. Он отвечает за:
- Регистрацию на Metadata Server
- Приём и хранение чанков от клиентов
- Отдачу чанков по запросу клиентов
- Мониторинг свободного места на диске
- Периодическую отправку keep-alive сообщений

## 2. Структура компонентов

### 2.1. Основные модули

```
storage-node/
├── src/
│   ├── main.cpp                 # Точка входа
│   ├── node.cpp                 # Основной класс узла
│   ├── chunk_storage.cpp        # Управление хранением чанков
│   ├── metadata_client.cpp      # Взаимодействие с Metadata Server
│   ├── protocol_handler.cpp     # Обработка протокола
│   └── disk_utils.cpp           # Работа с диском (WinAPI)
├── include/
│   ├── node.h
│   ├── chunk_storage.h
│   ├── metadata_client.h
│   ├── protocol_handler.h
│   ├── disk_utils.h
│   └── protocol.h
└── CMakeLists.txt
```

## 3. Классовая структура

### 3.1. Класс `StorageNode`

**Ответственность:** Основной класс узла, управляющий жизненным циклом

```cpp
class StorageNode {
private:
    std::string nodeId;
    std::string storagePath;          // Путь к директории хранения
    std::string metadataServerIp;
    int metadataServerPort;
    int listenPort;
    SOCKET listenSocket;
    ChunkStorage chunkStorage;
    MetadataClient metadataClient;
    ProtocolHandler protocolHandler;
    bool running;
    
public:
    bool Initialize(const std::string& storagePath,
                   const std::string& metadataServerIp,
                   int metadataServerPort);
    bool RegisterWithMetadataServer();
    void Run();
    void Shutdown();
    void HandleClient(SOCKET clientSocket);
    void StartKeepAliveThread();
};
```

### 3.2. Класс `ChunkStorage`

**Ответственность:** Управление хранением чанков на диске

```cpp
class ChunkStorage {
private:
    std::string storagePath;
    std::mutex storageMutex;
    std::unordered_map<std::string, std::string> chunkMap;  // chunkId -> filepath
    
public:
    bool Initialize(const std::string& path);
    bool StoreChunk(const std::string& chunkId, 
                   const std::vector<uint8_t>& data);
    bool GetChunk(const std::string& chunkId, 
                 std::vector<uint8_t>& data);
    bool DeleteChunk(const std::string& chunkId);
    bool ChunkExists(const std::string& chunkId);
    uint64_t GetTotalSize();
    uint64_t GetFreeSpace();
    std::vector<std::string> ListChunks();
};
```

**Методы работы с файлами (WinAPI):**

```cpp
bool ChunkStorage::StoreChunk(const std::string& chunkId,
                             const std::vector<uint8_t>& data) {
    std::string filepath = storagePath + "\\" + chunkId + ".chunk";
    
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

bool ChunkStorage::GetChunk(const std::string& chunkId,
                            std::vector<uint8_t>& data) {
    std::string filepath = storagePath + "\\" + chunkId + ".chunk";
    
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
```

### 3.3. Класс `MetadataClient`

**Ответственность:** Взаимодействие с Metadata Server

```cpp
class MetadataClient {
private:
    std::string serverIp;
    int serverPort;
    std::string nodeId;
    
public:
    bool Connect(const std::string& ip, int port);
    bool RegisterNode(const std::string& localIp, int localPort,
                     uint64_t freeSpace, std::string& outNodeId);
    bool SendKeepAlive(const std::string& nodeId);
    bool UpdateFreeSpace(const std::string& nodeId, uint64_t freeSpace);
    void Disconnect();
};
```

**Реализация регистрации:**

```cpp
bool MetadataClient::RegisterNode(const std::string& localIp, int localPort,
                                 uint64_t freeSpace, std::string& outNodeId) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        return false;
    }
    
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    inet_pton(AF_INET, serverIp.c_str(), &serverAddr.sin_addr);
    
    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(sock);
        return false;
    }
    
    std::string request = "REGISTER_NODE " + localIp + " " + 
                         std::to_string(localPort) + " " + 
                         std::to_string(freeSpace);
    
    send(sock, request.c_str(), static_cast<int>(request.length()), 0);
    
    char buffer[1024];
    int bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        std::string response(buffer);
        
        // Парсинг ответа: "REGISTER_RESPONSE OK <node_id>"
        // или "REGISTER_RESPONSE ERROR <message>"
        // ...
    }
    
    closesocket(sock);
    return true;
}
```

### 3.4. Класс `DiskUtils`

**Ответственность:** Работа с диском через WinAPI

```cpp
class DiskUtils {
public:
    static uint64_t GetFreeSpace(const std::string& path);
    static uint64_t GetTotalSpace(const std::string& path);
    static bool CreateDirectoryIfNotExists(const std::string& path);
};
```

**Реализация через WinAPI:**

```cpp
uint64_t DiskUtils::GetFreeSpace(const std::string& path) {
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
```

## 4. Сетевой слой

### 4.1. Инициализация сервера

```cpp
bool StorageNode::Initialize(...) {
    // Инициализация Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }
    
    // Создание сокета
    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        return false;
    }
    
    // Привязка к порту
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(listenPort);
    
    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(listenSocket);
        return false;
    }
    
    // Прослушивание
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listenSocket);
        return false;
    }
    
    // Инициализация хранилища
    if (!chunkStorage.Initialize(storagePath)) {
        return false;
    }
    
    return true;
}
```

### 4.2. Основной цикл

```cpp
void StorageNode::Run() {
    // Регистрация на Metadata Server
    if (!RegisterWithMetadataServer()) {
        std::cerr << "Failed to register with metadata server" << std::endl;
        return;
    }
    
    // Запуск keep-alive потока
    StartKeepAliveThread();
    
    // Основной цикл обработки клиентов
    while (running) {
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket != INVALID_SOCKET) {
            std::thread(&StorageNode::HandleClient, this, clientSocket).detach();
        }
    }
}
```

### 4.3. Обработка клиентских запросов

```cpp
void StorageNode::HandleClient(SOCKET clientSocket) {
    char buffer[4096];
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        std::string request(buffer);
        
        std::string response = protocolHandler.ProcessRequest(
            request, chunkStorage);
        
        send(clientSocket, response.c_str(), 
             static_cast<int>(response.length()), 0);
    }
    
    closesocket(clientSocket);
}
```

## 5. Протокол взаимодействия

### 5.1. Команды от Client

- `STORE_CHUNK <chunk_id> <size>` - Сохранение чанка
  - Следом отправляются бинарные данные чанка
  - Ответ: `STORE_RESPONSE OK` или `STORE_RESPONSE ERROR <message>`

- `GET_CHUNK <chunk_id>` - Получение чанка
  - Ответ: `GET_RESPONSE OK <size>` + бинарные данные
  - или `GET_RESPONSE ERROR <message>`

- `DELETE_CHUNK <chunk_id>` - Удаление чанка
  - Ответ: `DELETE_RESPONSE OK` или `DELETE_RESPONSE ERROR`

- `CHECK_CHUNK <chunk_id>` - Проверка наличия чанка
  - Ответ: `CHECK_RESPONSE EXISTS` или `CHECK_RESPONSE NOT_FOUND`

### 5.2. Команды к Metadata Server

- `REGISTER_NODE <ip> <port> <free_space>` - Регистрация
- `KEEP_ALIVE <node_id>` - Подтверждение активности
- `UPDATE_SPACE <node_id> <free_space>` - Обновление места

## 6. Keep-Alive механизм

```cpp
void StorageNode::StartKeepAliveThread() {
    std::thread([this]() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            
            // Отправка keep-alive
            uint64_t freeSpace = chunkStorage.GetFreeSpace();
            metadataClient.SendKeepAlive(nodeId);
            metadataClient.UpdateFreeSpace(nodeId, freeSpace);
        }
    }).detach();
}
```

## 7. Управление дисковым пространством

### 7.1. Проверка свободного места

- При регистрации: определение через `GetDiskFreeSpaceEx`
- Периодически: обновление при keep-alive
- Перед сохранением чанка: проверка достаточности места

### 7.2. Структура хранения

```
storage_path/
├── abc123def456...chunk
├── 789ghi012jkl...chunk
└── ...
```

Имя файла = `{chunk_id}.chunk`, где `chunk_id` - SHA-256 хеш содержимого.

## 8. Обработка ошибок

- Недостаточно места → `STORE_RESPONSE ERROR INSUFFICIENT_SPACE`
- Чанк не найден → `GET_RESPONSE ERROR NOT_FOUND`
- Ошибка записи/чтения → логирование и возврат ошибки
- Потеря связи с Metadata Server → попытки переподключения

## 9. Безопасность (опционально)

- Проверка размера чанка (максимальный лимит)
- Валидация chunk_id (формат SHA-256)
- Ограничение количества одновременных соединений

