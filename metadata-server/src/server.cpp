#include "server.h"

#include "network_utils.h"
#include <chrono>
#include <iostream>
#include <thread>

MetadataServer::MetadataServer(int port)
    : port(port), listenSocket(INVALID_SOCKET), running(false),
      protocolHandler(&nodeManager, &metadataManager) {}

MetadataServer::~MetadataServer() { Shutdown(); }

// Инициализация сервера
bool MetadataServer::Initialize() {
  // Инициализация сетевого слоя
  if (!InitializeNetwork()) {
    return false;
  }

  // Запуск keep-alive проверки для NodeManager
  nodeManager.StartKeepAliveChecker();

  // Инициализация ProtocolHandler (уже создан в конструкторе)
  // protocolHandler инициализирован через список инициализации

  // Создание слушающего сокета
  if (!CreateListenSocket()) {
    return false;
  }

  return true;
}

// Инициализация сетевого слоя
bool MetadataServer::InitializeNetwork() {
  return NetworkUtils::InitializeWinsock();
}

// Создание слушающего сокета
bool MetadataServer::CreateListenSocket() {
  // Создание сокета
  listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listenSocket == INVALID_SOCKET) {
    std::cerr << "Error: Failed to create socket" << std::endl;
    return false;
  }

  // Настройка опций сокета (переиспользование адреса)
  int opt = 1;
#ifdef _WIN32
  if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char *>(&opt), sizeof(opt)) ==
      SOCKET_ERROR) {
#else
  if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) ==
      SOCKET_ERROR) {
#endif
    std::cerr << "Error: Failed to set socket options" << std::endl;
    NetworkUtils::CloseSocket(listenSocket);
    return false;
  }

  // Привязка к порту
  sockaddr_in serverAddr{};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  serverAddr.sin_port = htons(port);

  if (bind(listenSocket, (sockaddr *)&serverAddr, sizeof(serverAddr)) ==
      SOCKET_ERROR) {
    std::cerr << "Error: Failed to bind socket to port " << port
              << std::endl;
    NetworkUtils::CloseSocket(listenSocket);
    return false;
  }

  // Прослушивание
  if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
    std::cerr << "Error: Failed to listen on socket" << std::endl;
    NetworkUtils::CloseSocket(listenSocket);
    return false;
  }

  std::cout << "Metadata server listening on port " << port << std::endl;
  return true;
}

// Основной цикл работы
void MetadataServer::Run() {
  running = true;

  // Запуск потока приёма соединений
  acceptThread = std::thread(&MetadataServer::AcceptLoop, this);

  // Ожидание завершения
  if (acceptThread.joinable()) {
    acceptThread.join();
  }
}

// Цикл приёма соединений
void MetadataServer::AcceptLoop() {
  std::cout << "Metadata server is running. Waiting for connections..."
            << std::endl;

  while (running) {
    SOCKET clientSocket = accept(listenSocket, NULL, NULL);

    if (clientSocket == INVALID_SOCKET) {
      if (running) {
        std::cerr << "Error: Failed to accept connection" << std::endl;
      }
      continue;
    }

    // Проверка количества активных потоков
    {
      std::lock_guard<std::mutex> lock(threadsMutex);
      if (clientThreads.size() >= MAX_CLIENTS) {
        std::cerr << "Warning: Maximum clients reached, rejecting connection"
                  << std::endl;
        NetworkUtils::CloseSocket(clientSocket);
        continue;
      }
    }

    // Создание потока для обработки клиента
    std::thread clientThread(ClientHandlerThread, this, clientSocket);
    clientThread.detach();

    // Добавление в список потоков (для отслеживания)
    {
      std::lock_guard<std::mutex> lock(threadsMutex);
      // Не сохраняем detached потоки, они завершатся сами
    }
  }
}

// Обработка клиента
void MetadataServer::HandleClient(SOCKET clientSocket) {
  // Установка таймаута
  NetworkUtils::SetSocketTimeout(clientSocket, SOCKET_TIMEOUT_SEC);

  // Получение IP клиента
  std::string clientIP = NetworkUtils::GetClientIP(clientSocket);
  std::cout << "Client connected: " << clientIP << std::endl;

  // Получение первой строки запроса для определения команды
  std::string firstLine;
  if (!NetworkUtils::ReceiveMessage(clientSocket, firstLine, 4096, 30)) {
    std::cerr << "Error: Failed to receive message from client" << std::endl;
    NetworkUtils::CloseSocket(clientSocket);
    return;
  }

  std::cout << "Received command: " << firstLine << std::endl;

  // Обработка через ProtocolHandler
  // Для UPLOAD_COMPLETE передаем пустую строку, так как команда будет прочитана внутри
  std::string response;
  if (firstLine.find("UPLOAD_COMPLETE") == 0) {
    // Для UPLOAD_COMPLETE нужно прочитать многострочное сообщение
    // Передаем первую строку и сокет для чтения остальных строк
    std::cout << "Processing UPLOAD_COMPLETE multiline request" << std::endl;
    response = protocolHandler.ProcessMultilineRequest(firstLine, clientSocket);
    std::cout << "UPLOAD_COMPLETE response: " << response.substr(0, 50) << std::endl;
  } else {
    // Для остальных команд используем обычную обработку
    response = protocolHandler.ProcessRequest(firstLine, clientSocket);
  }

  // Отправка ответа
  // Для многострочных ответов отправляем напрямую через send
  // (ответ уже содержит \r\n после каждой строки)
  // Проверяем, является ли ответ многострочным (содержит \r\n не только в конце)
  size_t firstCrlf = response.find("\r\n");
  if (firstCrlf != std::string::npos && 
      firstCrlf < response.length() - 2) {
    // Многострочный ответ - отправляем напрямую (без добавления \r\n)
    int bytesSent = send(clientSocket, response.c_str(), 
                        static_cast<int>(response.length()), 0);
    if (bytesSent != static_cast<int>(response.length())) {
      std::cerr << "Error: Failed to send response to client" << std::endl;
    } else {
      std::cout << "Sent multiline response (" << response.length() << " bytes)" << std::endl;
    }
  } else {
    // Однострочный ответ - используем SendMessage
    if (!NetworkUtils::SendMessage(clientSocket, response)) {
      std::cerr << "Error: Failed to send response to client" << std::endl;
    }
  }

  NetworkUtils::CloseSocket(clientSocket);
  std::cout << "Client disconnected: " << clientIP << std::endl;
}

// Статический метод для потока обработки клиента
void MetadataServer::ClientHandlerThread(MetadataServer *server,
                                         SOCKET clientSocket) {
  server->HandleClient(clientSocket);
}

// Корректное завершение
void MetadataServer::Shutdown() {
  if (!running) {
    return;
  }

  running = false;

  // Закрытие слушающего сокета
  if (listenSocket != INVALID_SOCKET) {
    NetworkUtils::CloseSocket(listenSocket);
    listenSocket = INVALID_SOCKET;
  }

  // Ожидание завершения потока приёма
  if (acceptThread.joinable()) {
    acceptThread.join();
  }

  // Очистка ресурсов
  Cleanup();

  NetworkUtils::CleanupWinsock();
}

// Очистка ресурсов
void MetadataServer::Cleanup() {
  // Остановка keep-alive проверки
  nodeManager.StopKeepAliveChecker();
}

