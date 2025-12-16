@echo off
setlocal enabledelayedexpansion

echo ========================================
echo CourseStore Build Script for Windows
echo ========================================
echo.

REM Проверка наличия CMake
where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake is not installed or not in PATH
    echo Please install CMake from https://cmake.org/download/
    exit /b 1
)

REM Проверка наличия компилятора
where cl >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: MSVC compiler not found in PATH
    echo Trying to use MinGW or other compiler...
    echo.
)

REM Создание директории для сборки
if not exist "build" (
    echo Creating build directory...
    mkdir build
)

cd build

REM Генерация проекта CMake
echo.
echo Generating CMake project...
cmake .. -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Trying with MinGW Makefiles...
    cmake .. -G "MinGW Makefiles"
    if %ERRORLEVEL% NEQ 0 (
        echo.
        echo ERROR: Failed to generate CMake project
        echo Please check your CMake and compiler installation
        exit /b 1
    )
)

REM Компиляция проекта
echo.
echo Building project...
cmake --build . --config Release
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Build failed
    exit /b 1
)

echo.
echo ========================================
echo Build completed successfully!
echo ========================================
echo.
echo Executables are located in:
echo   - Metadata Server: build\metadata-server\Release\metadata-server.exe
echo   - Storage Node: build\storage-node\Release\storage-node.exe
echo   - Client: build\client\Release\client.exe
echo   - GUI Client: build\client-gui\Release\client-gui.exe
echo.
echo To run the system:
echo   1. Start Metadata Server: build\metadata-server\Release\metadata-server.exe 8080
echo   2. Start Storage Node: build\storage-node\Release\storage-node.exe --storage-path .\storage --metadata-server 127.0.0.1 --metadata-port 8080 --port 9000
echo   3. Use Client: build\client\Release\client.exe --server 127.0.0.1 --port 8080 list
echo.
echo Or use the launcher scripts:
echo   - run-servers.bat  - Start all servers
echo   - run-demo.bat     - Start servers + console client
echo   - run-gui.bat      - Start servers + GUI client
echo.

cd ..

endlocal

