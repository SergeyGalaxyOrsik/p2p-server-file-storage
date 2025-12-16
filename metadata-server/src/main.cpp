#include "server.h"

#include <csignal>
#include <cstdlib>
#include <iostream>

static MetadataServer *g_server = nullptr;

// Обработчик сигналов
void SignalHandler(int signal) {
  if (g_server != nullptr) {
    std::cout << "\nReceived signal " << signal
              << ". Shutting down gracefully..." << std::endl;
    g_server->Shutdown();
  }
  exit(0);
}

int main(int argc, char *argv[]) {
  // Парсинг аргументов (пока простой)
  int port = 8080;
  if (argc > 1) {
    try {
      port = std::stoi(argv[1]);
    } catch (const std::exception &) {
      std::cerr << "Error: Invalid port number" << std::endl;
      return 1;
    }
  }

  // Создание экземпляра сервера
  MetadataServer server(port);
  g_server = &server;

  // Регистрация обработчиков сигналов
#ifdef _WIN32
  signal(SIGINT, SignalHandler);
  signal(SIGTERM, SignalHandler);
#else
  signal(SIGINT, SignalHandler);
  signal(SIGTERM, SignalHandler);
  signal(SIGPIPE, SIG_IGN); // Игнорируем SIGPIPE
#endif

  // Инициализация
  if (!server.Initialize()) {
    std::cerr << "Error: Failed to initialize metadata server" << std::endl;
    return 1;
  }

  std::cout << "Metadata server initialized successfully" << std::endl;

  // Запуск сервера
  server.Run();

  // Корректное завершение
  server.Shutdown();

  return 0;
}

