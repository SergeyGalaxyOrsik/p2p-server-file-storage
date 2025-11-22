# Сетевой протокол CourseStore

## 1. Общие принципы

### 1.1. Базовые характеристики

- **Транспорт:** TCP/IP
- **Формат:** Текстовый протокол (ASCII) для простоты отладки
- **Кодировка:** UTF-8
- **Разделитель команд:** Пробел
- **Разделитель строк:** `\r\n` (CRLF)
- **Конец сообщения:** Закрытие соединения или специальный маркер

### 1.2. Структура сообщений

Все команды имеют формат:
```
COMMAND [ARG1] [ARG2] ... [ARGN]
```

Ответы имеют формат:
```
RESPONSE [STATUS] [DATA]
```

Где:
- `COMMAND` / `RESPONSE` - название команды/ответа (заглавные буквы)
- `STATUS` - статус выполнения (`OK` или `ERROR`)
- `DATA` - дополнительные данные (зависит от команды)

## 2. Протокол Metadata Server

### 2.1. Команды от Storage Node

#### REGISTER_NODE
**Назначение:** Регистрация нового узла хранения

**Формат запроса:**
```
REGISTER_NODE <ip_address> <port> <free_space>
```

**Параметры:**
- `ip_address` - IP-адрес узла (IPv4, например: 192.168.1.100)
- `port` - Порт узла (число, например: 9000)
- `free_space` - Свободное место в байтах (число, например: 1073741824)

**Формат ответа (успех):**
```
REGISTER_RESPONSE OK <node_id>
```

**Формат ответа (ошибка):**
```
REGISTER_RESPONSE ERROR <message>
```

**Пример:**
```
Запрос:  REGISTER_NODE 192.168.1.100 9000 1073741824
Ответ:   REGISTER_RESPONSE OK abc123def456ghi789
```

#### KEEP_ALIVE
**Назначение:** Подтверждение активности узла

**Формат запроса:**
```
KEEP_ALIVE <node_id>
```

**Параметры:**
- `node_id` - Идентификатор узла (строка)

**Формат ответа:**
```
KEEP_ALIVE_RESPONSE OK
```

**Пример:**
```
Запрос:  KEEP_ALIVE abc123def456ghi789
Ответ:   KEEP_ALIVE_RESPONSE OK
```

#### UPDATE_SPACE
**Назначение:** Обновление информации о свободном месте

**Формат запроса:**
```
UPDATE_SPACE <node_id> <free_space>
```

**Параметры:**
- `node_id` - Идентификатор узла
- `free_space` - Новое значение свободного места в байтах

**Формат ответа:**
```
UPDATE_SPACE_RESPONSE OK
```

**Пример:**
```
Запрос:  UPDATE_SPACE abc123def456ghi789 1048576000
Ответ:   UPDATE_SPACE_RESPONSE OK
```

### 2.2. Команды от Client

#### REQUEST_UPLOAD
**Назначение:** Запрос на загрузку файла

**Формат запроса:**
```
REQUEST_UPLOAD <filename> <file_size>
```

**Параметры:**
- `filename` - Имя файла в системе (может содержать пробелы, экранирование не требуется, но рекомендуется использовать кавычки)
- `file_size` - Размер файла в байтах

**Формат ответа (успех):**
```
UPLOAD_RESPONSE OK <node_count>
<node_id_1> <ip_1> <port_1> <free_space_1>
<node_id_2> <ip_2> <port_2> <free_space_2>
...
<node_id_n> <ip_n> <port_n> <free_space_n>
```

**Формат ответа (ошибка):**
```
UPLOAD_RESPONSE ERROR <message>
```

**Пример:**
```
Запрос:  REQUEST_UPLOAD document.pdf 5242880
Ответ:   UPLOAD_RESPONSE OK 4
         node1 192.168.1.100 9000 1073741824
         node2 192.168.1.101 9000 2147483648
         node3 192.168.1.102 9000 536870912
         node4 192.168.1.103 9000 1073741824
```

#### UPLOAD_COMPLETE
**Назначение:** Уведомление о завершении загрузки

**Формат запроса:**
```
UPLOAD_COMPLETE <filename>
<chunk_id_1> <index_1> <size_1> <node_id_1_1> <node_id_1_2>
<chunk_id_2> <index_2> <size_2> <node_id_2_1> <node_id_2_2>
...
<chunk_id_n> <index_n> <size_n> <node_id_n_1> <node_id_n_2>
END_CHUNKS
```

**Параметры:**
- `filename` - Имя файла
- Для каждого чанка:
  - `chunk_id` - SHA-256 хеш чанка
  - `index` - Порядковый номер чанка (начиная с 0)
  - `size` - Размер чанка в байтах
  - `node_id_1`, `node_id_2` - Идентификаторы узлов, хранящих чанк

**Формат ответа:**
```
UPLOAD_COMPLETE_RESPONSE OK
```

**Пример:**
```
Запрос:  UPLOAD_COMPLETE document.pdf
         abc123def456... 0 1048576 node1 node2
         789ghi012jkl... 1 1048576 node3 node4
         mno345pqr678... 2 1048576 node1 node3
         END_CHUNKS
Ответ:   UPLOAD_COMPLETE_RESPONSE OK
```

#### REQUEST_DOWNLOAD
**Назначение:** Запрос на скачивание файла

**Формат запроса:**
```
REQUEST_DOWNLOAD <filename>
```

**Параметры:**
- `filename` - Имя файла в системе

**Формат ответа (успех):**
```
DOWNLOAD_RESPONSE OK <file_size> <chunk_count>
<chunk_id_1> <index_1> <size_1> <node_id_1_1> <node_id_1_2>
<chunk_id_2> <index_2> <size_2> <node_id_2_1> <node_id_2_2>
...
<chunk_id_n> <index_n> <size_n> <node_id_n_1> <node_id_n_2>
END_CHUNKS
```

**Формат ответа (ошибка):**
```
DOWNLOAD_RESPONSE ERROR <message>
```

**Пример:**
```
Запрос:  REQUEST_DOWNLOAD document.pdf
Ответ:   DOWNLOAD_RESPONSE OK 5242880 5
         abc123def456... 0 1048576 node1 node2
         789ghi012jkl... 1 1048576 node3 node4
         mno345pqr678... 2 1048576 node1 node3
         stu901vwx234... 3 1048576 node2 node4
         yza567bcd890... 4 1048576 node1 node4
         END_CHUNKS
```

#### LIST_FILES
**Назначение:** Получение списка файлов

**Формат запроса:**
```
LIST_FILES
```

**Формат ответа:**
```
LIST_FILES_RESPONSE OK <file_count>
<filename_1> <size_1>
<filename_2> <size_2>
...
<filename_n> <size_n>
END_FILES
```

**Пример:**
```
Запрос:  LIST_FILES
Ответ:   LIST_FILES_RESPONSE OK 3
         document.pdf 5242880
         image.jpg 2097152
         data.zip 10485760
         END_FILES
```

## 3. Протокол Storage Node

### 3.1. Команды от Client

#### STORE_CHUNK
**Назначение:** Сохранение чанка на узле

**Формат запроса:**
```
STORE_CHUNK <chunk_id> <size>
```

**Параметры:**
- `chunk_id` - SHA-256 хеш чанка (64 символа hex)
- `size` - Размер данных в байтах

**После команды отправляются бинарные данные:**
- Отправляется ровно `size` байт данных чанка

**Формат ответа (успех):**
```
STORE_RESPONSE OK
```

**Формат ответа (ошибка):**
```
STORE_RESPONSE ERROR <message>
```

**Возможные ошибки:**
- `INSUFFICIENT_SPACE` - Недостаточно места
- `INVALID_CHUNK_ID` - Некорректный chunk_id
- `WRITE_ERROR` - Ошибка записи

**Пример:**
```
Запрос:  STORE_CHUNK abc123def456... 1048576
         [бинарные данные 1048576 байт]
Ответ:   STORE_RESPONSE OK
```

#### GET_CHUNK
**Назначение:** Получение чанка с узла

**Формат запроса:**
```
GET_CHUNK <chunk_id>
```

**Параметры:**
- `chunk_id` - SHA-256 хеш чанка

**Формат ответа (успех):**
```
GET_RESPONSE OK <size>
```

**После ответа отправляются бинарные данные:**
- Отправляется ровно `size` байт данных чанка

**Формат ответа (ошибка):**
```
GET_RESPONSE ERROR <message>
```

**Возможные ошибки:**
- `NOT_FOUND` - Чанк не найден
- `READ_ERROR` - Ошибка чтения

**Пример:**
```
Запрос:  GET_CHUNK abc123def456...
Ответ:   GET_RESPONSE OK 1048576
         [бинарные данные 1048576 байт]
```

#### DELETE_CHUNK
**Назначение:** Удаление чанка

**Формат запроса:**
```
DELETE_CHUNK <chunk_id>
```

**Параметры:**
- `chunk_id` - SHA-256 хеш чанка

**Формат ответа:**
```
DELETE_RESPONSE OK
```
или
```
DELETE_RESPONSE ERROR <message>
```

**Пример:**
```
Запрос:  DELETE_CHUNK abc123def456...
Ответ:   DELETE_RESPONSE OK
```

#### CHECK_CHUNK
**Назначение:** Проверка наличия чанка

**Формат запроса:**
```
CHECK_CHUNK <chunk_id>
```

**Параметры:**
- `chunk_id` - SHA-256 хеш чанка

**Формат ответа:**
```
CHECK_RESPONSE EXISTS <size>
```
или
```
CHECK_RESPONSE NOT_FOUND
```

**Пример:**
```
Запрос:  CHECK_CHUNK abc123def456...
Ответ:   CHECK_RESPONSE EXISTS 1048576
```

## 4. Обработка бинарных данных

### 4.1. Передача чанков

При передаче бинарных данных (чанков):
1. Сначала отправляется текстовая команда с размером
2. Затем отправляются бинарные данные указанного размера
3. Получатель читает ровно указанное количество байт

### 4.2. Пример полного обмена (STORE_CHUNK)

**Клиент → Узел:**
```
STORE_CHUNK abc123def456... 1048576\r\n
[1048576 байт бинарных данных]
```

**Узел → Клиент:**
```
STORE_RESPONSE OK\r\n
```

### 4.3. Пример полного обмена (GET_CHUNK)

**Клиент → Узел:**
```
GET_CHUNK abc123def456...\r\n
```

**Узел → Клиент:**
```
GET_RESPONSE OK 1048576\r\n
[1048576 байт бинарных данных]
```

## 5. Коды ошибок

### 5.1. Общие ошибки

- `INVALID_COMMAND` - Неизвестная команда
- `INVALID_PARAMETERS` - Некорректные параметры
- `NETWORK_ERROR` - Сетевая ошибка
- `SERVER_ERROR` - Внутренняя ошибка сервера

### 5.2. Ошибки Metadata Server

- `NODE_NOT_FOUND` - Узел не найден
- `FILE_NOT_FOUND` - Файл не найден
- `INSUFFICIENT_NODES` - Недостаточно узлов
- `FILE_ALREADY_EXISTS` - Файл уже существует

### 5.3. Ошибки Storage Node

- `INSUFFICIENT_SPACE` - Недостаточно места
- `CHUNK_NOT_FOUND` - Чанк не найден
- `INVALID_CHUNK_ID` - Некорректный chunk_id
- `READ_ERROR` - Ошибка чтения
- `WRITE_ERROR` - Ошибка записи

## 6. Таймауты и обработка соединений

### 6.1. Таймауты

- **Соединение:** 30 секунд
- **Чтение данных:** 60 секунд
- **Keep-Alive интервал:** 30 секунд
- **Таймаут неактивного узла:** 60 секунд

### 6.2. Закрытие соединений

- Соединение закрывается после завершения обмена
- При ошибке соединение закрывается немедленно
- Keep-Alive соединения могут быть постоянными

## 7. Примеры полных сценариев

### 7.1. Сценарий загрузки файла

```
1. Client → Metadata Server:
   REQUEST_UPLOAD document.pdf 5242880

2. Metadata Server → Client:
   UPLOAD_RESPONSE OK 4
   node1 192.168.1.100 9000 1073741824
   node2 192.168.1.101 9000 2147483648
   node3 192.168.1.102 9000 536870912
   node4 192.168.1.103 9000 1073741824

3. Client → Node1:
   STORE_CHUNK abc123... 1048576
   [данные чанка 0]

4. Node1 → Client:
   STORE_RESPONSE OK

5. Client → Node2:
   STORE_CHUNK abc123... 1048576
   [данные чанка 0 - реплика]

6. Node2 → Client:
   STORE_RESPONSE OK

7. [Аналогично для остальных чанков...]

8. Client → Metadata Server:
   UPLOAD_COMPLETE document.pdf
   abc123... 0 1048576 node1 node2
   789ghi... 1 1048576 node3 node4
   ...
   END_CHUNKS

9. Metadata Server → Client:
   UPLOAD_COMPLETE_RESPONSE OK
```

### 7.2. Сценарий скачивания файла

```
1. Client → Metadata Server:
   REQUEST_DOWNLOAD document.pdf

2. Metadata Server → Client:
   DOWNLOAD_RESPONSE OK 5242880 5
   abc123... 0 1048576 node1 node2
   789ghi... 1 1048576 node3 node4
   ...
   END_CHUNKS

3. Client → Node1:
   GET_CHUNK abc123...

4. Node1 → Client:
   GET_RESPONSE OK 1048576
   [данные чанка 0]

5. [Параллельно] Client → Node3:
   GET_CHUNK 789ghi...

6. Node3 → Client:
   GET_RESPONSE OK 1048576
   [данные чанка 1]

7. [Аналогично для остальных чанков...]

8. Client собирает файл из чанков
```

## 8. Расширения протокола (опционально)

### 8.1. Аутентификация

В будущем можно добавить:
```
AUTH <username> <password>
AUTH_RESPONSE OK <token>
```

### 8.2. Сжатие данных

Можно добавить флаг сжатия:
```
STORE_CHUNK <chunk_id> <size> COMPRESSED
```

### 8.3. Шифрование

Можно добавить поддержку шифрования на уровне протокола.

