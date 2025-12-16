#pragma once

#include "core/download_manager.h"
#include "core/metadata_client.h"
#include "core/upload_manager.h"
#include <memory>
#include <string>
#include <vector>

class Client {
private:
  std::unique_ptr<MetadataClient> metadataClient;
  std::unique_ptr<UploadManager> uploadManager;
  std::unique_ptr<DownloadManager> downloadManager;

  std::string serverIp;
  int serverPort;

public:
  Client();
  ~Client();

  // Инициализация
  bool Initialize(const std::string &serverIp, int serverPort);
  void Shutdown();

  // Обработка команд
  bool ExecuteCommand(const std::vector<std::string> &args);
  bool HandleUpload(const std::vector<std::string> &args);
  bool HandleDownload(const std::vector<std::string> &args);
  bool HandleList(const std::vector<std::string> &args);
  bool HandleHelp(const std::vector<std::string> &args);

  // Утилиты
  void PrintUsage();
  void PrintError(const std::string &message);
  void PrintInfo(const std::string &message);
  bool ValidateCommand(const std::vector<std::string> &args);

  // Геттеры
  UploadManager *GetUploadManager() { return uploadManager.get(); }
  DownloadManager *GetDownloadManager() { return downloadManager.get(); }
  MetadataClient *GetMetadataClient() { return metadataClient.get(); }
};

