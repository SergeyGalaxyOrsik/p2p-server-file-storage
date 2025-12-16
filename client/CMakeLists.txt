cmake_minimum_required(VERSION 3.15)
project(client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
file(GLOB_RECURSE SOURCES
    src/*.cpp
)

# Platform specific definitions
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

# Create executable
add_executable(client ${SOURCES})

# Include directories
target_include_directories(client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

# Link libraries
if(WIN32)
    target_link_libraries(client PRIVATE ws2_32 crypt32)
elseif(UNIX)
    target_link_libraries(client PRIVATE pthread crypto)
endif()

# Link common library
target_link_libraries(client PRIVATE common)

