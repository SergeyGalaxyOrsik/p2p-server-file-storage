@echo off
setlocal enabledelayedexpansion

echo ========================================
echo CourseStore Servers Launcher
echo ========================================
echo.

REM Проверка наличия скомпилированных файлов
set METADATA_SERVER=build\metadata-server\Release\metadata-server.exe
set STORAGE_NODE=build\storage-node\Release\storage-node.exe

if not exist "%METADATA_SERVER%" (
    echo ERROR: Metadata server not found. Please run build.bat first.
    exit /b 1
)

if not exist "%STORAGE_NODE%" (
    echo ERROR: Storage node not found. Please run build.bat first.
    exit /b 1
)

REM Создание директорий для хранения
if not exist "storage1" mkdir storage1
if not exist "storage2" mkdir storage2
if not exist "storage3" mkdir storage3

echo Starting Metadata Server...
start "Metadata Server" cmd /k "cd /d %~dp0 && %METADATA_SERVER% 8080"
timeout /t 2 /nobreak >nul

echo Starting Storage Node 1...
start "Storage Node 1" cmd /k "cd /d %~dp0 && %STORAGE_NODE% --storage-path storage1 --metadata-server 127.0.0.1 --metadata-port 8080 --port 9000 --ip 127.0.0.1"
timeout /t 2 /nobreak >nul

echo Starting Storage Node 2...
start "Storage Node 2" cmd /k "cd /d %~dp0 && %STORAGE_NODE% --storage-path storage2 --metadata-server 127.0.0.1 --metadata-port 8080 --port 9001 --ip 127.0.0.1"
timeout /t 2 /nobreak >nul

echo Starting Storage Node 3...
start "Storage Node 3" cmd /k "cd /d %~dp0 && %STORAGE_NODE% --storage-path storage3 --metadata-server 127.0.0.1 --metadata-port 8080 --port 9002 --ip 127.0.0.1"
timeout /t 3 /nobreak >nul

echo.
echo ========================================
echo All servers started!
echo ========================================
echo.
echo Metadata Server: running on port 8080
echo Storage Node 1: running on port 9000
echo Storage Node 2: running on port 9001
echo Storage Node 3: running on port 9002
echo.
echo Press any key to stop all servers...
pause >nul

echo.
echo Stopping all servers...
taskkill /FI "WINDOWTITLE eq Metadata Server*" /F >nul 2>&1
taskkill /FI "WINDOWTITLE eq Storage Node*" /F >nul 2>&1

echo Done!

endlocal

