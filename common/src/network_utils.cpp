#include "network_utils.h"

#include <cstring>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <unistd.h>
#endif

namespace NetworkUtils {

bool InitializeWinsock() {
#ifdef _WIN32
  WSADATA wsaData;
  int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  return result == 0;
#else
  return true; // На Linux инициализация не требуется
#endif
}

void CleanupWinsock() {
#ifdef _WIN32
  WSACleanup();
#endif
}

bool SendMessage(SOCKET socket, const std::string &message) {
  std::string msg = message;
  // Добавляем \r\n если его нет
  if (msg.length() < 2 || msg.substr(msg.length() - 2) != "\r\n") {
    msg += "\r\n";
  }

  int bytesSent =
      send(socket, msg.c_str(), static_cast<int>(msg.length()), 0);
  return bytesSent == static_cast<int>(msg.length());
}

bool ReceiveMessage(SOCKET socket, std::string &message, size_t maxSize,
                   int timeoutSec) {
  message.clear();

  // Установка таймаута
  SetSocketTimeout(socket, timeoutSec);

  // Читаем по одному байту до нахождения \r\n
  // Это медленнее, но гарантирует, что мы не прочитаем лишние данные
  char prevChar = '\0';
  while (message.length() < maxSize) {
    char c;
    int bytesReceived = recv(socket, &c, 1, 0);

    if (bytesReceived == SOCKET_ERROR) {
#ifdef _WIN32
      int error = WSAGetLastError();
      if (error == WSAETIMEDOUT) {
        return false; // Таймаут
      }
#endif
      return false; // Ошибка
    }

    if (bytesReceived == 0) {
      break; // Соединение закрыто
    }

    // Проверяем на \r\n
    if (prevChar == '\r' && c == '\n') {
      // Найден \r\n - удаляем последний \r из message и выходим
      if (!message.empty() && message.back() == '\r') {
        message.pop_back();
      }
      break;
    }

    message += c;
    prevChar = c;
  }

  return !message.empty();
}

bool SendBinaryData(SOCKET socket, const void *data, size_t size) {
  if (size == 0) {
    return true;
  }

  size_t totalSent = 0;
  const char *dataPtr = static_cast<const char *>(data);

  while (totalSent < size) {
    int bytesSent = send(socket, dataPtr + totalSent,
                        static_cast<int>(size - totalSent), 0);

    if (bytesSent == SOCKET_ERROR) {
#ifdef _WIN32
      int error = WSAGetLastError();
      // Не выводим ошибку для таймаута, так как это может быть нормально
      if (error != WSAETIMEDOUT) {
        // Можно добавить отладочный вывод здесь при необходимости
      }
#endif
      return false;
    }

    totalSent += static_cast<size_t>(bytesSent);
  }

  // Убеждаемся, что все данные отправлены
  if (totalSent != size) {
    return false;
  }

  return true;
}

bool ReceiveBinaryData(SOCKET socket, void *buffer, size_t size,
                      int timeoutSec) {
  if (size == 0) {
    return true;
  }

  // Установка таймаута
  SetSocketTimeout(socket, timeoutSec);

  size_t totalReceived = 0;
  char *bufferPtr = static_cast<char *>(buffer);

  while (totalReceived < size) {
    int bytesReceived = recv(socket, bufferPtr + totalReceived,
                            static_cast<int>(size - totalReceived), 0);

    if (bytesReceived == SOCKET_ERROR) {
#ifdef _WIN32
      int error = WSAGetLastError();
      if (error == WSAETIMEDOUT) {
        return false; // Таймаут
      }
#endif
      return false; // Ошибка
    }

    if (bytesReceived == 0) {
      return false; // Соединение закрыто
    }

    totalReceived += static_cast<size_t>(bytesReceived);
  }

  return true;
}

std::string GetClientIP(SOCKET socket) {
  sockaddr_in clientAddr{};
  socklen_t addrLen = sizeof(clientAddr);

  if (getpeername(socket, (sockaddr *)&clientAddr, &addrLen) == 0) {
    char ipStr[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &(clientAddr.sin_addr), ipStr,
                  INET_ADDRSTRLEN) != NULL) {
      return std::string(ipStr);
    }
  }

  return "";
}

bool SetSocketTimeout(SOCKET socket, int seconds) {
#ifdef _WIN32
  DWORD timeout = seconds * 1000; // миллисекунды
  return setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char *>(&timeout),
                   sizeof(timeout)) == 0 &&
         setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO,
                   reinterpret_cast<const char *>(&timeout),
                   sizeof(timeout)) == 0;
#else
  struct timeval timeout;
  timeout.tv_sec = seconds;
  timeout.tv_usec = 0;
  return setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                   sizeof(timeout)) == 0 &&
         setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout,
                   sizeof(timeout)) == 0;
#endif
}

void CloseSocket(SOCKET socket) {
  if (socket != INVALID_SOCKET) {
    closesocket(socket);
  }
}

} // namespace NetworkUtils


