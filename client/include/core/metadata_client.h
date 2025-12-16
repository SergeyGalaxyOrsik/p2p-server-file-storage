#pragma once

#include "chunk_processor.h"

#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
typedef int SOCKET;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

struct StorageNodeInfo {
  std::string nodeId;
  std::string ipAddress;
  int port;
  uint64_t freeSpace;
};

struct FileMetadata {
  std::string filename;
  uint64_t totalSize;
  size_t chunkCount;
  struct ChunkInfo {
    std::string chunkId;
    size_t index;
    size_t size;
    std::vector<std::string> nodeIds; // Список nodeId для этого чанка
    // IP и порт узлов можно получить через отдельный запрос или кэш
  };
  std::vector<ChunkInfo> chunks;
};

// Структура для хранения полной информации об узле (для кэша)
struct NodeInfoCache {
  std::string nodeId;
  std::string ipAddress;
  int port;
  uint64_t freeSpace;
};

class MetadataClient {
private:
  std::string serverIp;
  int serverPort;
  // Кэш информации об узлах (nodeId -> NodeInfoCache)
  std::unordered_map<std::string, NodeInfoCache> nodeCache;

public:
  MetadataClient(const std::string &ip, int port);
  ~MetadataClient();

  // Базовые методы
  bool ConnectToServer(SOCKET &socket);
  bool SendRequest(SOCKET socket, const std::string &request);
  bool ReceiveResponse(SOCKET socket, std::string &response);

  // Загрузка
  std::vector<StorageNodeInfo> RequestUploadNodes(const std::string &filename,
                                                   uint64_t fileSize);
  bool NotifyUploadComplete(
      const std::string &filename,
      const std::vector<Chunk> &chunks,
      const std::vector<std::vector<std::string>> &chunkNodeIds);

  // Скачивание и список
  FileMetadata RequestDownload(const std::string &filename);
  std::vector<std::pair<std::string, uint64_t>> ListFiles();
  std::vector<StorageNodeInfo> ListNodes();
  bool TestConnection();

  // Получение информации об узле из кэша
  bool GetNodeInfo(const std::string &nodeId, StorageNodeInfo &nodeInfo);

private:
  // Внутренние методы
  std::vector<std::string> ParseCommand(const std::string &command);
  std::vector<std::string> SplitLines(const std::string &text);
  StorageNodeInfo ParseNodeInfo(const std::vector<std::string> &args);
};

