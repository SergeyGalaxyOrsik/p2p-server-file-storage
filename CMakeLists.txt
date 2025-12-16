cmake_minimum_required(VERSION 3.15)
project(CourseStore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Общие настройки
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7+
endif()

# Подпроекты
add_subdirectory(common)
add_subdirectory(metadata-server)
add_subdirectory(storage-node)
add_subdirectory(client)
add_subdirectory(client-gui)

