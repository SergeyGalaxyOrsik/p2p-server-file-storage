#include "core/client.h"

#include <iostream>

Client::Client() : serverPort(0) {}

Client::~Client() { Shutdown(); }

// Инициализация клиента
bool Client::Initialize(const std::string &serverIp, int serverPort) {
  this->serverIp = serverIp;
  this->serverPort = serverPort;

  // Создание MetadataClient
  metadataClient = std::make_unique<MetadataClient>(serverIp, serverPort);

  // Проверка подключения к Metadata Server
  if (!metadataClient->TestConnection()) {
    PrintError("Failed to connect to metadata server at " + serverIp + ":" +
               std::to_string(serverPort));
    return false;
  }

  PrintInfo("Connected to metadata server at " + serverIp + ":" +
            std::to_string(serverPort));

  // Создание UploadManager и DownloadManager
  uploadManager = std::make_unique<UploadManager>(metadataClient.get());
  downloadManager = std::make_unique<DownloadManager>(metadataClient.get());

  return true;
}

// Корректное завершение
void Client::Shutdown() {
  downloadManager.reset();
  uploadManager.reset();
  metadataClient.reset();
}

// Выполнение команды
bool Client::ExecuteCommand(const std::vector<std::string> &args) {
  if (args.empty()) {
    PrintUsage();
    return false;
  }

  std::string command = args[0];

  if (command == "upload") {
    return HandleUpload(args);
  } else if (command == "download") {
    return HandleDownload(args);
  } else if (command == "list") {
    return HandleList(args);
  } else if (command == "help" || command == "--help" || command == "-h") {
    return HandleHelp(args);
  } else {
    PrintError("Unknown command: " + command);
    PrintUsage();
    return false;
  }
}

// Обработка команды upload
bool Client::HandleUpload(const std::vector<std::string> &args) {
  if (args.size() < 3) {
    PrintError("Usage: upload <local_path> <remote_filename>");
    return false;
  }

  std::string localPath = args[1];
  std::string remoteFilename = args[2];

  PrintInfo("Uploading file: " + localPath + " as " + remoteFilename);

  if (!uploadManager->UploadFile(localPath, remoteFilename)) {
    PrintError("Failed to upload file");
    return false;
  }

  PrintInfo("File uploaded successfully");
  return true;
}

// Обработка команды download
bool Client::HandleDownload(const std::vector<std::string> &args) {
  if (args.size() < 3) {
    PrintError("Usage: download <remote_filename> <local_path>");
    return false;
  }

  std::string remoteFilename = args[1];
  std::string localPath = args[2];

  PrintInfo("Downloading file: " + remoteFilename + " to " + localPath);

  if (!downloadManager->DownloadFile(remoteFilename, localPath)) {
    PrintError("Failed to download file");
    return false;
  }

  PrintInfo("File downloaded successfully");
  return true;
}

// Обработка команды list
bool Client::HandleList(const std::vector<std::string> &args) {
  PrintInfo("Requesting file list from server...");

  auto files = metadataClient->ListFiles();

  if (files.empty()) {
    PrintInfo("No files found");
    return true;
  }

  std::cout << "\nFiles in storage:" << std::endl;
  std::cout << "----------------------------------------" << std::endl;
  for (const auto &file : files) {
    std::cout << file.first << " (" << file.second << " bytes)" << std::endl;
  }
  std::cout << "----------------------------------------" << std::endl;
  std::cout << "Total: " << files.size() << " files" << std::endl;

  return true;
}

// Обработка команды help
bool Client::HandleHelp(const std::vector<std::string> &args) {
  PrintUsage();
  return true;
}

// Вывод справки
void Client::PrintUsage() {
  std::cout << "\nCourseStore Client - Usage:\n" << std::endl;
  std::cout << "Commands:" << std::endl;
  std::cout << "  upload <local_path> <remote_filename>  - Upload a file"
            << std::endl;
  std::cout << "  download <remote_filename> <local_path>  - Download a file"
            << std::endl;
  std::cout << "  list  - List all files in storage" << std::endl;
  std::cout << "  help  - Show this help message" << std::endl;
  std::cout << "\nOptions:" << std::endl;
  std::cout << "  --server <ip>  - Metadata server IP address" << std::endl;
  std::cout << "  --port <port>  - Metadata server port" << std::endl;
  std::cout << "  --verbose      - Verbose output" << std::endl;
  std::cout << "  --quiet        - Quiet output" << std::endl;
  std::cout << std::endl;
}

// Вывод ошибки
void Client::PrintError(const std::string &message) {
  std::cerr << "Error: " << message << std::endl;
}

// Вывод информации
void Client::PrintInfo(const std::string &message) {
  std::cout << message << std::endl;
}

// Валидация команды
bool Client::ValidateCommand(const std::vector<std::string> &args) {
  if (args.empty()) {
    return false;
  }

  std::string command = args[0];

  if (command == "upload" && args.size() < 3) {
    return false;
  }
  if (command == "download" && args.size() < 3) {
    return false;
  }

  return true;
}


