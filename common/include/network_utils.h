#pragma once

#include <string>

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

namespace NetworkUtils {
  // Инициализация Winsock
  bool InitializeWinsock();
  void CleanupWinsock();

  // Работа с сообщениями
  bool SendMessage(SOCKET socket, const std::string &message);
  bool ReceiveMessage(SOCKET socket, std::string &message,
                     size_t maxSize = 4096, int timeoutSec = 30);

  // Работа с бинарными данными
  bool SendBinaryData(SOCKET socket, const void *data, size_t size);
  bool ReceiveBinaryData(SOCKET socket, void *buffer, size_t size,
                        int timeoutSec = 60);

  // Утилиты
  std::string GetClientIP(SOCKET socket);
  bool SetSocketTimeout(SOCKET socket, int seconds);
  void CloseSocket(SOCKET socket);
}

