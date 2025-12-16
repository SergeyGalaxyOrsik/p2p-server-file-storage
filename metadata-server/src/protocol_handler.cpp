#include "protocol_handler.h"

#include "network_utils.h"
#include <algorithm>
#include <iostream>
#include <sstream>

// Конструктор
ProtocolHandler::ProtocolHandler(NodeManager *nodeManager,
                                 MetadataManager *metadataManager)
    : nodeManager(nodeManager), metadataManager(metadataManager) {}

// Главный метод обработки запроса
std::string ProtocolHandler::ProcessRequest(const std::string &request,
                                            SOCKET socket) {
  if (request.empty()) {
    return CreateErrorResponse("INVALID_COMMAND", "Empty request");
  }

  // Парсинг команды
  std::vector<std::string> args = ParseCommand(request);

  if (args.empty()) {
    return CreateErrorResponse("INVALID_COMMAND", "No command specified");
  }

  std::string command = args[0];

  // Маршрутизация к обработчику
  // UPLOAD_COMPLETE обрабатывается отдельно через ProcessMultilineRequest
  if (command == "REGISTER_NODE") {
    return HandleRegisterNode(args);
  } else if (command == "KEEP_ALIVE") {
    return HandleKeepAlive(args);
  } else if (command == "UPDATE_SPACE") {
    return HandleUpdateSpace(args);
  } else if (command == "REQUEST_UPLOAD") {
    return HandleRequestUpload(args);
  } else if (command == "REQUEST_DOWNLOAD") {
    return HandleRequestDownload(args);
  } else if (command == "LIST_FILES") {
    return HandleListFiles();
  } else if (command == "LIST_NODES") {
    return HandleListNodes();
  } else {
    return CreateErrorResponse("INVALID_COMMAND",
                               "Unknown command: " + command);
  }
}

// Парсинг команды
std::vector<std::string> ProtocolHandler::ParseCommand(
    const std::string &command) {
  std::vector<std::string> tokens;
  std::stringstream ss(command);
  std::string token;

  while (ss >> token) {
    tokens.push_back(token);
  }

  return tokens;
}

// Создание ответа об ошибке
std::string ProtocolHandler::CreateErrorResponse(const std::string &errorCode,
                                                const std::string &message) {
  return "ERROR " + errorCode + " " + message + "\r\n";
}

// Создание успешного ответа
std::string ProtocolHandler::CreateSuccessResponse(const std::string &data) {
  return data + "\r\n";
}

// Разделение текста на строки
std::vector<std::string> ProtocolHandler::SplitLines(const std::string &text) {
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

// Чтение многострочного запроса
bool ProtocolHandler::ReadMultilineRequest(SOCKET socket,
                                          std::string &request,
                                          const std::string &firstLine) {
  request.clear();
  
  // Добавляем первую строку (уже прочитанную)
  request += firstLine + "\r\n";

  // Чтение остальных строк до END_CHUNKS или END_FILES
  std::string line;
  int lineCount = 0;
  while (true) {
    if (!NetworkUtils::ReceiveMessage(socket, line, 4096, 30)) {
      std::cerr << "Error: Failed to read line " << (lineCount + 1) << " in multiline request" << std::endl;
      return false;
    }

    request += line + "\r\n";
    lineCount++;

    // Проверка на завершение
    if (line == "END_CHUNKS" || line == "END_FILES") {
      break;
    }
    
    // Защита от бесконечного цикла
    if (lineCount > 10000) {
      std::cerr << "Error: Too many lines in multiline request" << std::endl;
      return false;
    }
  }

  return true;
}

// Обработка REGISTER_NODE
std::string ProtocolHandler::HandleRegisterNode(
    const std::vector<std::string> &args) {
  // Валидация аргументов
  if (args.size() != 4) {
    return "REGISTER_RESPONSE ERROR INVALID_PARAMETERS\r\n";
  }

  // Парсинг параметров
  std::string ip = args[1];
  int port;
  uint64_t freeSpace;

  try {
    port = std::stoi(args[2]);
    freeSpace = std::stoull(args[3]);
  } catch (const std::exception &) {
    return "REGISTER_RESPONSE ERROR INVALID_PARAMETERS\r\n";
  }

  // Регистрация узла
  std::string nodeId;
  if (nodeManager->RegisterNode(ip, port, freeSpace, nodeId)) {
    return "REGISTER_RESPONSE OK " + nodeId + "\r\n";
  } else {
    return "REGISTER_RESPONSE ERROR REGISTRATION_FAILED\r\n";
  }
}

// Обработка KEEP_ALIVE
std::string ProtocolHandler::HandleKeepAlive(
    const std::vector<std::string> &args) {
  // Валидация аргументов
  if (args.size() != 2) {
    return "KEEP_ALIVE_RESPONSE ERROR INVALID_PARAMETERS\r\n";
  }

  std::string nodeId = args[1];

  // Обновление времени последнего контакта
  nodeManager->UpdateNodeLastSeen(nodeId);

  return "KEEP_ALIVE_RESPONSE OK\r\n";
}

// Обработка UPDATE_SPACE
std::string ProtocolHandler::HandleUpdateSpace(
    const std::vector<std::string> &args) {
  // Валидация аргументов
  if (args.size() != 3) {
    return "UPDATE_SPACE_RESPONSE ERROR INVALID_PARAMETERS\r\n";
  }

  std::string nodeId = args[1];
  uint64_t freeSpace;

  try {
    freeSpace = std::stoull(args[2]);
  } catch (const std::exception &) {
    return "UPDATE_SPACE_RESPONSE ERROR INVALID_PARAMETERS\r\n";
  }

  // Обновление свободного места
  if (nodeManager->UpdateNodeSpace(nodeId, freeSpace)) {
    return "UPDATE_SPACE_RESPONSE OK\r\n";
  } else {
    return "UPDATE_SPACE_RESPONSE ERROR NODE_NOT_FOUND\r\n";
  }
}

// Обработка REQUEST_UPLOAD
std::string ProtocolHandler::HandleRequestUpload(
    const std::vector<std::string> &args) {
  // Валидация аргументов
  if (args.size() < 3) {
    return "UPLOAD_RESPONSE ERROR INVALID_PARAMETERS\r\n";
  }

  // Парсинг имени файла (может содержать пробелы)
  std::string filename = args[1];
  for (size_t i = 2; i < args.size() - 1; ++i) {
    filename += " " + args[i];
  }

  uint64_t fileSize;
  try {
    fileSize = std::stoull(args.back());
  } catch (const std::exception &) {
    return "UPLOAD_RESPONSE ERROR INVALID_PARAMETERS\r\n";
  }

  // Вычисление необходимого количества узлов
  // Для каждого чанка нужно REPLICATION_FACTOR узлов
  // Предполагаем размер чанка 1MB (1048576 байт)
  const size_t CHUNK_SIZE = 1048576;
  size_t chunkCount = (fileSize + CHUNK_SIZE - 1) / CHUNK_SIZE;
  size_t requiredNodes = chunkCount * REPLICATION_FACTOR;

  std::cout << "REQUEST_UPLOAD: filename=" << filename 
            << ", fileSize=" << fileSize 
            << ", chunkCount=" << chunkCount 
            << ", requiredNodes=" << requiredNodes << std::endl;

  // Получение доступных узлов
  std::vector<StorageNode> nodes =
      nodeManager->GetAvailableNodes(requiredNodes, CHUNK_SIZE);

  std::cout << "REQUEST_UPLOAD: found " << nodes.size() 
            << " available nodes (need at least " << REPLICATION_FACTOR << ")" << std::endl;

  if (nodes.size() < REPLICATION_FACTOR) {
    std::cerr << "Error: Not enough nodes. Found " << nodes.size() 
              << ", need at least " << REPLICATION_FACTOR << std::endl;
    return "UPLOAD_RESPONSE ERROR INSUFFICIENT_NODES\r\n";
  }

  // Формирование ответа
  std::stringstream response;
  response << "UPLOAD_RESPONSE OK " << nodes.size() << "\r\n";

  for (const auto &node : nodes) {
    response << node.nodeId << " " << node.ipAddress << " " << node.port << " "
             << node.freeSpace << "\r\n";
  }

  return response.str();
}

// Обработка многострочного запроса
std::string ProtocolHandler::ProcessMultilineRequest(const std::string &firstLine, SOCKET socket) {
  // Проверяем, что это UPLOAD_COMPLETE
  std::vector<std::string> args = ParseCommand(firstLine);
  if (args.empty() || args[0] != "UPLOAD_COMPLETE") {
    return CreateErrorResponse("INVALID_COMMAND", "Expected UPLOAD_COMPLETE");
  }
  
  return HandleUploadComplete(firstLine, socket);
}

// Обработка UPLOAD_COMPLETE
std::string ProtocolHandler::HandleUploadComplete(const std::string &firstLine, SOCKET socket) {
  std::string request;
  if (!ReadMultilineRequest(socket, request, firstLine)) {
    return "UPLOAD_COMPLETE_RESPONSE ERROR READ_ERROR\r\n";
  }

  // Разделение на строки
  std::vector<std::string> lines = SplitLines(request);
  if (lines.empty()) {
    return "UPLOAD_COMPLETE_RESPONSE ERROR INVALID_FORMAT\r\n";
  }

  // Парсинг первой строки: UPLOAD_COMPLETE <filename>
  std::vector<std::string> firstLineArgs = ParseCommand(lines[0]);
  if (firstLineArgs.size() < 2 || firstLineArgs[0] != "UPLOAD_COMPLETE") {
    return "UPLOAD_COMPLETE_RESPONSE ERROR INVALID_FORMAT\r\n";
  }

  std::string filename = firstLineArgs[1];
  for (size_t i = 2; i < firstLineArgs.size(); ++i) {
    filename += " " + firstLineArgs[i];
  }

  // Парсинг чанков
  std::vector<ChunkInfo> chunks;
  uint64_t totalSize = 0;

  for (size_t i = 1; i < lines.size(); ++i) {
    if (lines[i] == "END_CHUNKS") {
      break;
    }

    std::vector<std::string> chunkArgs = ParseCommand(lines[i]);
    if (chunkArgs.size() < 5) {
      continue; // Пропускаем некорректные строки
    }

    ChunkInfo chunk;
    chunk.chunkId = chunkArgs[0];

    try {
      chunk.index = std::stoull(chunkArgs[1]);
      chunk.size = std::stoull(chunkArgs[2]);
    } catch (const std::exception &) {
      continue; // Пропускаем некорректные строки
    }

    // Добавление nodeIds
    for (size_t j = 3; j < chunkArgs.size(); ++j) {
      chunk.nodeIds.push_back(chunkArgs[j]);
    }

    if (chunk.IsValid()) {
      chunks.push_back(chunk);
      totalSize += chunk.size;
    }
  }

  // Регистрация файла
  if (metadataManager->RegisterFile(filename, totalSize, chunks)) {
    return "UPLOAD_COMPLETE_RESPONSE OK\r\n";
  } else {
    return "UPLOAD_COMPLETE_RESPONSE ERROR REGISTRATION_FAILED\r\n";
  }
}

// Обработка REQUEST_DOWNLOAD
std::string ProtocolHandler::HandleRequestDownload(
    const std::vector<std::string> &args) {
  // Валидация аргументов
  if (args.size() < 2) {
    return "DOWNLOAD_RESPONSE ERROR INVALID_PARAMETERS\r\n";
  }

  // Парсинг имени файла (может содержать пробелы)
  std::string filename = args[1];
  for (size_t i = 2; i < args.size(); ++i) {
    filename += " " + args[i];
  }

  // Получение метаданных файла
  FileMetadata *metadata = metadataManager->GetFileMetadata(filename);
  if (metadata == nullptr) {
    return "DOWNLOAD_RESPONSE ERROR FILE_NOT_FOUND\r\n";
  }

  // Формирование ответа
  std::stringstream response;
  response << "DOWNLOAD_RESPONSE OK " << metadata->totalSize << " "
           << metadata->chunks.size() << "\r\n";

  for (const auto &chunk : metadata->chunks) {
    response << chunk.chunkId << " " << chunk.index << " " << chunk.size;
    for (const auto &nodeId : chunk.nodeIds) {
      // Получение информации об узле для включения IP и порта
      StorageNode *node = nodeManager->GetNode(nodeId);
      if (node != nullptr) {
        // Формат: nodeId ip port (для каждого узла)
        response << " " << nodeId << " " << node->ipAddress << " "
                 << node->port;
      } else {
        // Если узел не найден, возвращаем только nodeId
        response << " " << nodeId;
      }
    }
    response << "\r\n";
  }

  response << "END_CHUNKS\r\n";

  return response.str();
}

// Обработка LIST_NODES
std::string ProtocolHandler::HandleListNodes() {
  // Получение всех активных узлов
  std::vector<StorageNode> activeNodes = nodeManager->GetAllActiveNodes();

  // Формирование ответа
  std::stringstream response;
  response << "LIST_NODES_RESPONSE OK " << activeNodes.size() << "\r\n";

  for (const auto &node : activeNodes) {
    response << node.nodeId << " " << node.ipAddress << " " << node.port << " "
             << node.freeSpace << " " << (node.isActive ? "1" : "0") << "\r\n";
  }

  response << "END_NODES\r\n";

  return response.str();
}

// Обработка LIST_FILES
std::string ProtocolHandler::HandleListFiles() {
  // Получение списка файлов
  std::vector<std::string> fileList = metadataManager->ListFiles();

  // Формирование ответа
  std::stringstream response;
  response << "LIST_FILES_RESPONSE OK " << fileList.size() << "\r\n";

  for (const auto &filename : fileList) {
    FileMetadata *metadata = metadataManager->GetFileMetadata(filename);
    if (metadata != nullptr) {
      response << filename << " " << metadata->totalSize << "\r\n";
    }
  }

  response << "END_FILES\r\n";

  return response.str();
}

