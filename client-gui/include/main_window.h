#pragma once

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <commctrl.h>

#include "chunk_info.h"

#include <string>
#include <vector>
#include <memory>

// Forward declarations
class MetadataClient;
class UploadManager;
class DownloadManager;
class ChunkViewer;

struct FileInfo {
    std::string filename;
    uint64_t size;
    size_t chunkCount;
};

struct NodeInfo {
    std::string nodeId;
    std::string ipAddress;
    int port;
    uint64_t freeSpace;
    bool isActive;
};

// ChunkInfo defined in chunk_info.h

class MainWindow {
private:
    HWND hwnd;
    HINSTANCE hInstance;
    
    // Controls
    HWND hListFiles;
    HWND hListNodes;
    HWND hBtnUpload;
    HWND hBtnDownload;
    HWND hBtnRefresh;
    HWND hBtnViewChunks;
    HWND hStatusBar;
    HWND hEditServer;
    HWND hEditPort;
    HWND hBtnConnect;
    HWND hLabelFiles;
    HWND hLabelNodes;
    
    // Client components
    std::unique_ptr<MetadataClient> metadataClient;
    std::unique_ptr<UploadManager> uploadManager;
    std::unique_ptr<DownloadManager> downloadManager;
    
    // Data
    std::vector<FileInfo> files;
    std::vector<NodeInfo> nodes;
    std::string selectedFile;
    bool isConnected;
    
    // Constants
    static const int ID_LIST_FILES = 1001;
    static const int ID_LIST_NODES = 1002;
    static const int ID_BTN_UPLOAD = 1003;
    static const int ID_BTN_DOWNLOAD = 1004;
    static const int ID_BTN_REFRESH = 1005;
    static const int ID_BTN_VIEW_CHUNKS = 1006;
    static const int ID_EDIT_SERVER = 1007;
    static const int ID_EDIT_PORT = 1008;
    static const int ID_BTN_CONNECT = 1009;
    
public:
    MainWindow(HINSTANCE hInstance);
    ~MainWindow();
    
    bool Create();
    void Show(int nCmdShow);
    HWND GetHandle() const { return hwnd; }
    
    // Message handlers
    void OnCreate();
    void OnCommand(WPARAM wParam, LPARAM lParam);
    void OnSize(int width, int height);
    void OnClose();
    void OnDestroy();
    
    // UI Updates
    void UpdateFileList();
    void UpdateNodeList();
    void UpdateStatus(const std::string& message);
    
    // Actions
    void ConnectToServer();
    void DisconnectFromServer();
    void RefreshFiles();
    void RefreshNodes();
    void UploadFile();
    void DownloadFile();
    void ViewChunks();
    void ShowChunkLocations(const std::string& filename);
    
private:
    // Helper methods
    void CreateControls();
    void LayoutControls(int width, int height);
    std::string GetSelectedFile();
    void ShowMessage(const std::string& title, const std::string& message, UINT type = MB_OK);
    std::string ShowOpenFileDialog();
    std::string ShowSaveFileDialog(const std::string& defaultName);
    
    // Static window procedure
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

