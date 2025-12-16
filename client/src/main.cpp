#include "core/client.h"

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include <iostream>
#include <string>
#include <vector>

void PrintUsage(const char *programName) {
  std::cout << "Usage: " << programName
            << " --server <ip> --port <port> <command> [args...]" << std::endl;
  std::cout << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  --server <ip>     Metadata server IP address" << std::endl;
  std::cout << "  --port <port>     Metadata server port" << std::endl;
  std::cout << "  --verbose         Verbose output" << std::endl;
  std::cout << "  --quiet           Quiet output" << std::endl;
  std::cout << std::endl;
  std::cout << "Commands:" << std::endl;
  std::cout << "  upload <local_path> <remote_filename>  - Upload a file"
            << std::endl;
  std::cout << "  download <remote_filename> <local_path>  - Download a file"
            << std::endl;
  std::cout << "  list  - List all files in storage" << std::endl;
  std::cout << "  help  - Show this help message" << std::endl;
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
  // Initialize Winsock
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    std::cerr << "Error: Failed to initialize Winsock" << std::endl;
    return 1;
  }
#endif

  if (argc < 2) {
    PrintUsage(argv[0]);
#ifdef _WIN32
    WSACleanup();
#endif
    return 1;
  }

  // Парсинг аргументов
  std::string serverIp;
  int serverPort = 0;
  bool verbose = false;
  bool quiet = false;
  std::vector<std::string> commandArgs;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];

    if (arg == "--server" && i + 1 < argc) {
      serverIp = argv[++i];
    } else if (arg == "--port" && i + 1 < argc) {
      try {
        serverPort = std::stoi(argv[++i]);
      } catch (const std::exception &) {
        std::cerr << "Error: Invalid port number" << std::endl;
        return 1;
      }
    } else if (arg == "--verbose") {
      verbose = true;
    } else if (arg == "--quiet") {
      quiet = true;
    } else if (arg == "--help" || arg == "-h") {
      PrintUsage(argv[0]);
      return 0;
    } else {
      // Это команда или аргумент команды
      commandArgs.push_back(arg);
    }
  }

  // Проверка обязательных параметров
  if (serverIp.empty() || serverPort == 0) {
    std::cerr << "Error: --server and --port are required" << std::endl;
    PrintUsage(argv[0]);
    return 1;
  }

  // Проверка наличия команды
  if (commandArgs.empty()) {
    std::cerr << "Error: No command specified" << std::endl;
    PrintUsage(argv[0]);
    return 1;
  }

  // Создание экземпляра Client
  Client client;

  // Инициализация клиента
  if (!client.Initialize(serverIp, serverPort)) {
    std::cerr << "Error: Failed to initialize client" << std::endl;
    return 1;
  }

  // Выполнение команды
  bool success = client.ExecuteCommand(commandArgs);

  // Корректное завершение
  client.Shutdown();

#ifdef _WIN32
  WSACleanup();
#endif

  return success ? 0 : 1;
}


