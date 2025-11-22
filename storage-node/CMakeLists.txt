cmake_minimum_required(VERSION 3.15)
project(storage-node LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
target_include_directories(storage-node PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/core
    ${CMAKE_CURRENT_SOURCE_DIR}/include/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

# Source files
file(GLOB_RECURSE SOURCES
    src/core/*.cpp
)

# Platform specific sources and libraries
if(WIN32)
    file(GLOB_RECURSE PLATFORM_SOURCES src/platform/windows/*.cpp)
    list(APPEND SOURCES ${PLATFORM_SOURCES})
    
    add_definitions(-D_WIN32_WINNT=0x0601)
    
    add_executable(storage-node ${SOURCES})
    target_link_libraries(storage-node PRIVATE ws2_32)
elseif(UNIX)
    file(GLOB_RECURSE PLATFORM_SOURCES src/platform/linux/*.cpp)
    list(APPEND SOURCES ${PLATFORM_SOURCES})
    
    add_executable(storage-node ${SOURCES})
    target_link_libraries(storage-node PRIVATE pthread)
endif()

# Link common library
# Assuming common library is built in the parent directory or available as a target
# If 'common' is a subdirectory in the root project, it should be linked here.
# For now, we assume it's available as a target named 'common'.
target_link_libraries(storage-node PRIVATE common)
