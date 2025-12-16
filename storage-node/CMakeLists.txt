cmake_minimum_required(VERSION 3.15)
project(storage-node LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
file(GLOB_RECURSE SOURCES
    src/core/*.cpp
    src/main.cpp
)

# Platform specific sources
file(GLOB_RECURSE PLATFORM_SOURCES 
    src/platform/factory.cpp
)
if(WIN32)
    file(GLOB_RECURSE WINDOWS_SOURCES src/platform/windows/*.cpp)
    list(APPEND PLATFORM_SOURCES ${WINDOWS_SOURCES})
elseif(UNIX)
    file(GLOB_RECURSE LINUX_SOURCES src/platform/linux/*.cpp)
    list(APPEND PLATFORM_SOURCES ${LINUX_SOURCES})
endif()
list(APPEND SOURCES ${PLATFORM_SOURCES})

# Platform specific definitions and libraries
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

# Create executable
add_executable(storage-node ${SOURCES})

# Include directories
target_include_directories(storage-node PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

# Link libraries
if(WIN32)
    target_link_libraries(storage-node PRIVATE ws2_32 iphlpapi)
elseif(UNIX)
    target_link_libraries(storage-node PRIVATE pthread)
endif()

# Link common library
target_link_libraries(storage-node PRIVATE common)
