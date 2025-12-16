#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
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

#include "node_manager.h"
#include "metadata_manager.h"
#include "protocol_handler.h"

class MetadataServer {
private:
  // Сетевые компоненты
  SOCKET listenSocket;
  int port;
  std::atomic<bool> running;

  // Менеджеры
  NodeManager nodeManager;
  MetadataManager metadataManager;
  ProtocolHandler protocolHandler; // Инициализируется в конструкторе

  // Потоки
  std::thread acceptThread;
  std::vector<std::thread> clientThreads;
  std::mutex threadsMutex;

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
  static void ClientHandlerThread(MetadataServer *server,
                                  SOCKET clientSocket);

  // Геттеры
  NodeManager &GetNodeManager() { return nodeManager; }
  MetadataManager &GetMetadataManager() { return metadataManager; }

private:
  // Внутренние методы
  bool InitializeNetwork();
  bool CreateListenSocket();
  void AcceptLoop();
  void Cleanup();
};

