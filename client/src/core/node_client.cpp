#include "core/node_client.h"

#include "core/metadata_client.h"
#include "network_utils.h"
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <netdb.h>
#endif

NodeClient::NodeClient() = default;

NodeClient::~NodeClient() = default;

// Подключение к узлу
bool NodeClient::ConnectToNode(const StorageNodeInfo &node, SOCKET &socket) {
  // Winsock должен быть инициализирован до вызова этого метода
  socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket == INVALID_SOCKET) {
    return false;
  }

  sockaddr_in serverAddr{};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(node.port);

  if (inet_pton(AF_INET, node.ipAddress.c_str(), &serverAddr.sin_addr) != 1) {
    closesocket(socket);
    return false;
  }

  if (connect(socket, (sockaddr *)&serverAddr, sizeof(serverAddr)) ==
      SOCKET_ERROR) {
    closesocket(socket);
    return false;
  }

  return true;
}

// Отправка бинарных данных
bool NodeClient::SendBinaryData(SOCKET socket,
                                const std::vector<uint8_t> &data) {
  return NetworkUtils::SendBinaryData(socket, data.data(), data.size());
}

// Получение бинарных данных
bool NodeClient::ReceiveBinaryData(SOCKET socket, std::vector<uint8_t> &data,
                                  size_t size) {
  data.resize(size);
  return NetworkUtils::ReceiveBinaryData(socket, data.data(), size);
}

// Сохранение чанка
bool NodeClient::StoreChunk(const StorageNodeInfo &node,
                            const std::string &chunkId,
                            const std::vector<uint8_t> &data) {
  SOCKET socket = INVALID_SOCKET;

  std::cout << "Connecting to storage node " << node.nodeId 
            << " at " << node.ipAddress << ":" << node.port << std::endl;

  if (!ConnectToNode(node, socket)) {
    std::cerr << "Error: Failed to connect to storage node " << node.nodeId 
              << " at " << node.ipAddress << ":" << node.port << std::endl;
    return false;
  }

  std::cout << "Connected to storage node " << node.nodeId << std::endl;

  // Формирование команды
  std::string command = "STORE_CHUNK " + chunkId + " " +
                       std::to_string(data.size());

  std::cout << "Sending command: " << command << std::endl;

  // Отправка команды
  if (!NetworkUtils::SendMessage(socket, command)) {
    std::cerr << "Error: Failed to send command to storage node" << std::endl;
    closesocket(socket);
    return false;
  }

  std::cout << "Sending binary data: " << data.size() << " bytes" << std::endl;

  // Отправка бинарных данных
  if (!SendBinaryData(socket, data)) {
    std::cerr << "Error: Failed to send binary data to storage node" << std::endl;
    closesocket(socket);
    return false;
  }

  std::cout << "Binary data sent successfully (" << data.size() << " bytes)" << std::endl;
  std::cout << "Waiting for response from storage node" << std::endl;

  // Получение ответа
  std::string response;
  bool success = NetworkUtils::ReceiveMessage(socket, response);

  if (success) {
    std::cout << "Received response: " << response << std::endl;
  } else {
    std::cerr << "Error: Failed to receive response from storage node" << std::endl;
  }

  closesocket(socket);

  // Проверка ответа
  return success && response.find("STORE_RESPONSE OK") != std::string::npos;
}

// Получение чанка
bool NodeClient::GetChunk(const StorageNodeInfo &node,
                         const std::string &chunkId,
                         std::vector<uint8_t> &data) {
  SOCKET socket = INVALID_SOCKET;

  if (!ConnectToNode(node, socket)) {
    return false;
  }

  // Формирование команды
  std::string command = "GET_CHUNK " + chunkId;

  // Отправка команды
  if (!NetworkUtils::SendMessage(socket, command)) {
    closesocket(socket);
    return false;
  }

  // Получение ответа с размером
  std::string response;
  if (!NetworkUtils::ReceiveMessage(socket, response)) {
    closesocket(socket);
    return false;
  }

  // Парсинг ответа: GET_RESPONSE OK <size>
  std::vector<std::string> args;
  std::stringstream ss(response);
  std::string token;
  while (ss >> token) {
    args.push_back(token);
  }

  if (args.size() < 3 || args[0] != "GET_RESPONSE" || args[1] != "OK") {
    closesocket(socket);
    return false;
  }

  size_t size;
  try {
    size = std::stoull(args[2]);
  } catch (const std::exception &) {
    closesocket(socket);
    return false;
  }

  // Получение бинарных данных
  bool success = ReceiveBinaryData(socket, data, size);

  closesocket(socket);

  return success;
}

// Проверка наличия чанка
bool NodeClient::CheckChunk(const StorageNodeInfo &node,
                            const std::string &chunkId) {
  SOCKET socket = INVALID_SOCKET;

  if (!ConnectToNode(node, socket)) {
    return false;
  }

  // Формирование команды
  std::string command = "CHECK_CHUNK " + chunkId;

  // Отправка команды
  if (!NetworkUtils::SendMessage(socket, command)) {
    closesocket(socket);
    return false;
  }

  // Получение ответа
  std::string response;
  bool success = NetworkUtils::ReceiveMessage(socket, response);

  closesocket(socket);

  // Проверка ответа
  return success && response.find("CHECK_RESPONSE EXISTS") !=
                        std::string::npos;
}

