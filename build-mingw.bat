@echo off
setlocal enabledelayedexpansion

echo ========================================
echo CourseStore Build Script for Windows (MinGW)
echo ========================================
echo.

REM Проверка наличия CMake
where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake is not installed or not in PATH
    echo Please install CMake from https://cmake.org/download/
    exit /b 1
)

REM Проверка наличия MinGW
where g++ >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: MinGW g++ compiler not found in PATH
    echo Please install MinGW-w64 and add it to PATH
    exit /b 1
)

REM Создание директории для сборки
if not exist "build" (
    echo Creating build directory...
    mkdir build
)

cd build

REM Генерация проекта CMake с MinGW
echo.
echo Generating CMake project with MinGW...
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Failed to generate CMake project
    exit /b 1
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
echo   - Metadata Server: build\metadata-server.exe
echo   - Storage Node: build\storage-node.exe
echo   - Client: build\client.exe
echo   - GUI Client: build\client-gui.exe
echo.

cd ..

endlocal

