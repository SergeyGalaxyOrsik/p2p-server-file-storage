#pragma once

#include "metadata_manager.h"
#include "node_manager.h"
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
typedef int SOCKET;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

class ProtocolHandler {
private:
  NodeManager *nodeManager;
  MetadataManager *metadataManager;

  // Константы протокола (используем строковые литералы напрямую)

  // Константы для репликации
  static constexpr size_t REPLICATION_FACTOR = 2; // 2 копии каждого чанка

public:
  ProtocolHandler(NodeManager *nodeManager, MetadataManager *metadataManager);

  // Основной метод обработки
  std::string ProcessRequest(const std::string &request, SOCKET socket);
  
  // Обработка многострочного запроса (для UPLOAD_COMPLETE)
  std::string ProcessMultilineRequest(const std::string &firstLine, SOCKET socket);

private:
  // Обработчики команд от Storage Node
  std::string HandleRegisterNode(const std::vector<std::string> &args);
  std::string HandleKeepAlive(const std::vector<std::string> &args);
  std::string HandleUpdateSpace(const std::vector<std::string> &args);

  // Обработчики команд от Client
  std::string HandleRequestUpload(const std::vector<std::string> &args);
  std::string HandleUploadComplete(const std::string &firstLine, SOCKET socket);
  std::string HandleRequestDownload(const std::vector<std::string> &args);
  std::string HandleListFiles();
  std::string HandleListNodes();

  // Утилиты
  std::vector<std::string> ParseCommand(const std::string &command);
  std::string CreateErrorResponse(const std::string &errorCode,
                                  const std::string &message);
  std::string CreateSuccessResponse(const std::string &data);
  bool ReadMultilineRequest(SOCKET socket, std::string &request, const std::string &firstLine);
  std::vector<std::string> SplitLines(const std::string &text);
};

