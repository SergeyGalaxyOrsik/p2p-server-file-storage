cmake_minimum_required(VERSION 3.15)
project(client-gui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows specific
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

# Source files
file(GLOB_RECURSE SOURCES
    src/*.cpp
)

# Create executable
add_executable(client-gui WIN32 ${SOURCES})

# Include directories
target_include_directories(client-gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

# Link libraries
if(WIN32)
    target_link_libraries(client-gui PRIVATE
        ws2_32
        crypt32
        comctl32
        common
    )
endif()

# Link client library (нужно будет создать статическую библиотеку из client)
# Пока используем прямое включение исходников
file(GLOB CLIENT_SOURCES
    ../client/src/core/*.cpp
)

target_sources(client-gui PRIVATE ${CLIENT_SOURCES})

