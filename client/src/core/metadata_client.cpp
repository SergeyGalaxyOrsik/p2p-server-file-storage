#include "core/metadata_client.h"

#include "core/chunk_processor.h"
#include "network_utils.h"
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <netdb.h>
#endif

MetadataClient::MetadataClient(const std::string &ip, int port)
    : serverIp(ip), serverPort(port) {}

MetadataClient::~MetadataClient() = default;

// Подключение к серверу
bool MetadataClient::ConnectToServer(SOCKET &socket) {
  // Winsock должен быть инициализирован до вызова этого метода
  socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket == INVALID_SOCKET) {
    return false;
  }

  sockaddr_in serverAddr{};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(serverPort);

  if (inet_pton(AF_INET, serverIp.c_str(), &serverAddr.sin_addr) != 1) {
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

// Отправка запроса
bool MetadataClient::SendRequest(SOCKET socket, const std::string &request) {
  return NetworkUtils::SendMessage(socket, request);
}

// Получение ответа
bool MetadataClient::ReceiveResponse(SOCKET socket, std::string &response) {
  return NetworkUtils::ReceiveMessage(socket, response);
}

// Парсинг команды
std::vector<std::string> MetadataClient::ParseCommand(
    const std::string &command) {
  std::vector<std::string> tokens;
  std::stringstream ss(command);
  std::string token;

  while (ss >> token) {
    tokens.push_back(token);
  }

  return tokens;
}

// Разделение текста на строки
std::vector<std::string> MetadataClient::SplitLines(const std::string &text) {
  std::vector<std::string> lines;
  std::stringstream ss(text);
  std::string line;

  while (std::getline(ss, line)) {
    // Удаление \r если есть
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (!line.empty()) {
      lines.push_back(line);
    }
  }

  return lines;
}

// Парсинг информации об узле
StorageNodeInfo MetadataClient::ParseNodeInfo(
    const std::vector<std::string> &args) {
  StorageNodeInfo node;
  if (args.size() >= 4) {
    node.nodeId = args[0];
    node.ipAddress = args[1];
    try {
      node.port = std::stoi(args[2]);
      node.freeSpace = std::stoull(args[3]);
    } catch (const std::exception &) {
      // Ошибка парсинга
    }
  }
  return node;
}

// Запрос узлов для загрузки
std::vector<StorageNodeInfo> MetadataClient::RequestUploadNodes(
    const std::string &filename, uint64_t fileSize) {
  std::vector<StorageNodeInfo> nodes;
  SOCKET socket = INVALID_SOCKET;

  if (!ConnectToServer(socket)) {
    return nodes;
  }

  // Формирование запроса
  std::string request = "REQUEST_UPLOAD " + filename + " " +
                       std::to_string(fileSize);

  // Отправка запроса
  if (!SendRequest(socket, request)) {
    closesocket(socket);
    return nodes;
  }

  // Получение многострочного ответа
  // Читаем весь ответ сразу через recv, так как metadata server отправляет его одним пакетом
  std::string fullResponse;
  char buffer[4096];
  
  // Читаем данные из сокета до закрытия соединения
  while (true) {
    int bytesReceived = recv(socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesReceived == SOCKET_ERROR) {
#ifdef _WIN32
      int error = WSAGetLastError();
      if (error == WSAETIMEDOUT || error == WSAECONNRESET) {
        break; // Таймаут или соединение закрыто
      }
#endif
      break; // Ошибка
    }
    
    if (bytesReceived == 0) {
      break; // Соединение закрыто
    }
    
    buffer[bytesReceived] = '\0';
    fullResponse += std::string(buffer, bytesReceived);
    
    // Если получили достаточно данных (предполагаем, что ответ не очень большой)
    if (fullResponse.length() > 4096) {
      break;
    }
  }

  closesocket(socket);

  std::cout << "Received full response (" << fullResponse.length() << " bytes): " << fullResponse.substr(0, 100) << std::endl;

  // Парсинг ответа
  std::vector<std::string> lines = SplitLines(fullResponse);
  std::cout << "Total lines received: " << lines.size() << std::endl;
  if (lines.empty()) {
    std::cerr << "Error: No lines in response" << std::endl;
    return nodes;
  }

  // Первая строка: UPLOAD_RESPONSE OK <node_count>
  std::vector<std::string> firstLineArgs = ParseCommand(lines[0]);
  if (firstLineArgs.size() < 3 || firstLineArgs[0] != "UPLOAD_RESPONSE" ||
      firstLineArgs[1] != "OK") {
    std::cerr << "Error: Invalid UPLOAD_RESPONSE format. First line: " << lines[0] << std::endl;
    return nodes;
  }

  // Парсинг узлов
  for (size_t i = 1; i < lines.size(); ++i) {
    std::vector<std::string> nodeArgs = ParseCommand(lines[i]);
    std::cout << "Parsing node line " << i << ": " << lines[i] << " -> " << nodeArgs.size() << " args" << std::endl;
    if (nodeArgs.size() >= 4) {
      StorageNodeInfo node = ParseNodeInfo(nodeArgs);
      if (!node.nodeId.empty()) {
        std::cout << "Added node: " << node.nodeId << " at " << node.ipAddress << ":" << node.port << std::endl;
        nodes.push_back(node);
      }
    } else {
      std::cerr << "Warning: Node line has insufficient arguments: " << nodeArgs.size() << std::endl;
    }
  }

  std::cout << "Total nodes parsed: " << nodes.size() << std::endl;

  return nodes;
}

// Уведомление о завершении загрузки
bool MetadataClient::NotifyUploadComplete(
    const std::string &filename, const std::vector<Chunk> &chunks,
    const std::vector<std::vector<std::string>> &chunkNodeIds) {
  SOCKET socket = INVALID_SOCKET;

  if (!ConnectToServer(socket)) {
    return false;
  }

  // Формирование многострочного запроса
  std::stringstream request;
  request << "UPLOAD_COMPLETE " << filename << "\r\n";

  for (size_t i = 0; i < chunks.size(); ++i) {
    const auto &chunk = chunks[i];
    request << chunk.chunkId << " " << chunk.index << " " << chunk.size;

    // Добавление nodeIds
    if (i < chunkNodeIds.size()) {
      for (const auto &nodeId : chunkNodeIds[i]) {
        request << " " << nodeId;
      }
    }

    request << "\r\n";
  }

  request << "END_CHUNKS\r\n";

  // Отправка многострочного запроса напрямую (без добавления \r\n в конце)
  std::string requestStr = request.str();
  // NetworkUtils::SendMessage добавляет \r\n, но у нас уже есть \r\n в конце
  // Поэтому отправляем напрямую через send
  int bytesSent = send(socket, requestStr.c_str(), static_cast<int>(requestStr.length()), 0);
  if (bytesSent != static_cast<int>(requestStr.length())) {
    closesocket(socket);
    return false;
  }

  // Получение ответа
  std::string response;
  bool success = ReceiveResponse(socket, response);

  closesocket(socket);

  // Проверка ответа
  return success && response.find("UPLOAD_COMPLETE_RESPONSE OK") !=
                        std::string::npos;
}

// Запрос метаданных для скачивания
FileMetadata MetadataClient::RequestDownload(const std::string &filename) {
  FileMetadata metadata;
  SOCKET socket = INVALID_SOCKET;

  if (!ConnectToServer(socket)) {
    return metadata;
  }

  // Формирование запроса
  std::string request = "REQUEST_DOWNLOAD " + filename;

  // Отправка запроса
  if (!SendRequest(socket, request)) {
    closesocket(socket);
    return metadata;
  }

  // Получение многострочного ответа
  std::string response;
  std::string line;

  // Чтение первой строки
  if (!ReceiveResponse(socket, line)) {
    closesocket(socket);
    return metadata;
  }
  response += line + "\r\n";

  // Чтение остальных строк до END_CHUNKS
  while (true) {
    if (!ReceiveResponse(socket, line)) {
      break;
    }
    response += line + "\r\n";
    if (line == "END_CHUNKS") {
      break;
    }
  }

  closesocket(socket);

  // Парсинг ответа
  std::vector<std::string> lines = SplitLines(response);
  if (lines.empty()) {
    return metadata;
  }

  // Первая строка: DOWNLOAD_RESPONSE OK <file_size> <chunk_count>
  std::vector<std::string> firstLine = ParseCommand(lines[0]);
  if (firstLine.size() < 4 || firstLine[0] != "DOWNLOAD_RESPONSE" ||
      firstLine[1] != "OK") {
    return metadata;
  }

  try {
    metadata.totalSize = std::stoull(firstLine[2]);
    metadata.chunkCount = std::stoull(firstLine[3]);
  } catch (const std::exception &) {
    return metadata;
  }

  metadata.filename = filename;

  // Парсинг чанков
  for (size_t i = 1; i < lines.size(); ++i) {
    if (lines[i] == "END_CHUNKS") {
      break;
    }

    std::vector<std::string> chunkArgs = ParseCommand(lines[i]);
    if (chunkArgs.size() >= 4) {
      FileMetadata::ChunkInfo chunk;
      chunk.chunkId = chunkArgs[0];
      try {
        chunk.index = std::stoull(chunkArgs[1]);
        chunk.size = std::stoull(chunkArgs[2]);
      } catch (const std::exception &) {
        continue;
      }

      // Парсинг узлов: формат nodeId ip port (расширенный формат)
      size_t j = 3;
      while (j < chunkArgs.size()) {
        if (j + 2 < chunkArgs.size()) {
          // Расширенный формат: nodeId ip port
          std::string nodeId = chunkArgs[j];
          std::string ip = chunkArgs[j + 1];
          int port = 0;
          try {
            port = std::stoi(chunkArgs[j + 2]);
          } catch (const std::exception &) {
            // Ошибка парсинга порта
            j++;
            continue;
          }

          chunk.nodeIds.push_back(nodeId);

          // Сохранение в кэш
          NodeInfoCache nodeInfo;
          nodeInfo.nodeId = nodeId;
          nodeInfo.ipAddress = ip;
          nodeInfo.port = port;
          nodeInfo.freeSpace = 0; // FreeSpace не передается в REQUEST_DOWNLOAD
          nodeCache[nodeId] = nodeInfo;

          j += 3; // Переходим к следующему узлу
        } else {
          // Старый формат: только nodeId
          chunk.nodeIds.push_back(chunkArgs[j]);
          j++;
        }
      }

      metadata.chunks.push_back(chunk);
    }
  }

  return metadata;
}

// Список файлов
std::vector<std::pair<std::string, uint64_t>> MetadataClient::ListFiles() {
  std::vector<std::pair<std::string, uint64_t>> files;
  SOCKET socket = INVALID_SOCKET;

  if (!ConnectToServer(socket)) {
    return files;
  }

  // Формирование запроса
  std::string request = "LIST_FILES";

  // Отправка запроса
  if (!SendRequest(socket, request)) {
    closesocket(socket);
    return files;
  }

  // Получение многострочного ответа
  std::string response;
  std::string line;

  // Чтение первой строки
  if (!ReceiveResponse(socket, line)) {
    closesocket(socket);
    return files;
  }
  response += line + "\r\n";

  // Чтение остальных строк до END_FILES
  while (true) {
    if (!ReceiveResponse(socket, line)) {
      break;
    }
    response += line + "\r\n";
    if (line == "END_FILES") {
      break;
    }
  }

  closesocket(socket);

  // Парсинг ответа
  std::vector<std::string> lines = SplitLines(response);
  if (lines.empty()) {
    return files;
  }

  // Первая строка: LIST_FILES_RESPONSE OK <file_count>
  std::vector<std::string> firstLine = ParseCommand(lines[0]);
  if (firstLine.size() < 3 || firstLine[0] != "LIST_FILES_RESPONSE" ||
      firstLine[1] != "OK") {
    return files;
  }

  // Парсинг файлов
  for (size_t i = 1; i < lines.size(); ++i) {
    if (lines[i] == "END_FILES") {
      break;
    }

    std::vector<std::string> fileArgs = ParseCommand(lines[i]);
    if (fileArgs.size() >= 2) {
      try {
        uint64_t size = std::stoull(fileArgs[1]);
        files.push_back({fileArgs[0], size});
      } catch (const std::exception &) {
        continue;
      }
    }
  }

  return files;
}

// Список узлов
std::vector<StorageNodeInfo> MetadataClient::ListNodes() {
  std::vector<StorageNodeInfo> nodes;
  SOCKET socket = INVALID_SOCKET;

  if (!ConnectToServer(socket)) {
    return nodes;
  }

  // Формирование запроса
  std::string request = "LIST_NODES";

  // Отправка запроса
  if (!SendRequest(socket, request)) {
    closesocket(socket);
    return nodes;
  }

  // Получение многострочного ответа
  std::string response;
  std::string line;

  // Чтение первой строки
  if (!ReceiveResponse(socket, line)) {
    closesocket(socket);
    return nodes;
  }
  response += line + "\r\n";

  // Чтение остальных строк до END_NODES
  while (true) {
    if (!ReceiveResponse(socket, line)) {
      break;
    }
    response += line + "\r\n";
    if (line == "END_NODES") {
      break;
    }
  }

  closesocket(socket);

  // Парсинг ответа
  std::vector<std::string> lines = SplitLines(response);
  if (lines.empty()) {
    return nodes;
  }

  // Первая строка: LIST_NODES_RESPONSE OK <node_count>
  std::vector<std::string> firstLine = ParseCommand(lines[0]);
  if (firstLine.size() < 3 || firstLine[0] != "LIST_NODES_RESPONSE" ||
      firstLine[1] != "OK") {
    return nodes;
  }

  // Парсинг узлов: формат nodeId ip port freeSpace isActive
  for (size_t i = 1; i < lines.size(); ++i) {
    if (lines[i] == "END_NODES") {
      break;
    }

    std::vector<std::string> nodeArgs = ParseCommand(lines[i]);
    if (nodeArgs.size() >= 5) {
      StorageNodeInfo node;
      node.nodeId = nodeArgs[0];
      node.ipAddress = nodeArgs[1];
      try {
        node.port = std::stoi(nodeArgs[2]);
        node.freeSpace = std::stoull(nodeArgs[3]);
      } catch (const std::exception &) {
        continue;
      }
      
      if (!node.nodeId.empty()) {
        nodes.push_back(node);
        
        // Сохранение в кэш
        NodeInfoCache nodeInfo;
        nodeInfo.nodeId = node.nodeId;
        nodeInfo.ipAddress = node.ipAddress;
        nodeInfo.port = node.port;
        nodeInfo.freeSpace = node.freeSpace;
        nodeCache[node.nodeId] = nodeInfo;
      }
    }
  }

  return nodes;
}

// Проверка подключения
bool MetadataClient::TestConnection() {
  SOCKET socket = INVALID_SOCKET;
  if (!ConnectToServer(socket)) {
    return false;
  }
  closesocket(socket);
  return true;
}

// Получение информации об узле из кэша
bool MetadataClient::GetNodeInfo(const std::string &nodeId,
                                 StorageNodeInfo &nodeInfo) {
  auto it = nodeCache.find(nodeId);
  if (it != nodeCache.end()) {
    nodeInfo.nodeId = it->second.nodeId;
    nodeInfo.ipAddress = it->second.ipAddress;
    nodeInfo.port = it->second.port;
    nodeInfo.freeSpace = it->second.freeSpace;
    return true;
  }
  return false;
}

