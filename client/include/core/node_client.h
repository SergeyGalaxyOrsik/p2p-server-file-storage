#pragma once

#include <string>
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

// Forward declaration
struct StorageNodeInfo;

class NodeClient {
public:
  NodeClient();
  ~NodeClient();

  // Работа с чанками
  bool StoreChunk(const StorageNodeInfo &node, const std::string &chunkId,
                  const std::vector<uint8_t> &data);
  bool GetChunk(const StorageNodeInfo &node, const std::string &chunkId,
                std::vector<uint8_t> &data);
  bool CheckChunk(const StorageNodeInfo &node, const std::string &chunkId);

private:
  // Внутренние методы
  bool ConnectToNode(const StorageNodeInfo &node, SOCKET &socket);
  bool SendBinaryData(SOCKET socket, const std::vector<uint8_t> &data);
  bool ReceiveBinaryData(SOCKET socket, std::vector<uint8_t> &data,
                        size_t size);
};


