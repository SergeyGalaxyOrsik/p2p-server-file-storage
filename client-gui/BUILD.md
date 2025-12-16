# Сборка GUI клиента

## Требования

- Windows 10/11
- CMake 3.15+
- Visual Studio 2022 (рекомендуется) или MinGW-w64
- Скомпилированные библиотеки `common` и исходники `client`

## Сборка

GUI клиент компилируется вместе с остальными компонентами проекта:

```cmd
# Из корневой директории проекта
build.bat
```

Или для MinGW:

```cmd
build-mingw.bat
```

## Результат

После успешной сборки исполняемый файл будет находиться в:
- **Visual Studio:** `build\Release\client-gui.exe`
- **MinGW:** `build\client-gui.exe`

## Запуск

```cmd
build\Release\client-gui.exe
```

## Структура проекта

```
client-gui/
├── CMakeLists.txt          # Конфигурация сборки
├── include/
│   ├── main_window.h       # Главное окно приложения
│   └── chunk_viewer.h      # Окно просмотра чанков
├── src/
│   ├── main.cpp            # Точка входа (WinMain)
│   └── main_window.cpp     # Реализация главного окна
└── README.md               # Документация
```

## Зависимости

GUI клиент использует:
- **common** - общие утилиты (NetworkUtils, HashUtils)
- **client** - клиентская логика (MetadataClient, UploadManager, DownloadManager)

Все зависимости компилируются автоматически при сборке проекта.

## Особенности сборки

- Использует `/SUBSYSTEM:WINDOWS` для создания GUI приложения (без консоли)
- Связывается с библиотеками: `ws2_32`, `crypt32`, `comctl32`
- Включает исходники из `client/src/core/` для использования клиентской логики

## Устранение проблем

### Ошибка: "Cannot find metadata_client.h"

**Решение:** Убедитесь, что пути в `CMakeLists.txt` правильные и `client` проект скомпилирован.

### Ошибка: "Unresolved external symbol"

**Решение:** Проверьте, что все необходимые библиотеки указаны в `target_link_libraries`.

### Ошибка при запуске: "The application was unable to start correctly"

**Решение:** 
- Убедитесь, что все DLL зависимости доступны
- Проверьте, что Winsock инициализирован правильно
- Запустите от имени администратора (если требуется)


