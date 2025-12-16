cmake_minimum_required(VERSION 3.15)
project(metadata-server LANGUAGES CXX)

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
add_executable(metadata-server ${SOURCES})

# Include directories
target_include_directories(metadata-server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

# Link libraries
if(WIN32)
    target_link_libraries(metadata-server PRIVATE ws2_32)
elseif(UNIX)
    target_link_libraries(metadata-server PRIVATE pthread)
endif()

# Link common library
target_link_libraries(metadata-server PRIVATE common)

