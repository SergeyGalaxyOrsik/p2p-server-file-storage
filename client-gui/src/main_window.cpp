#include "main_window.h"

#include <commdlg.h>
#ifdef GetOpenFileName
#undef GetOpenFileName
#endif
#ifdef GetSaveFileName
#undef GetSaveFileName
#endif

#include "chunk_viewer.h"
#include "core/metadata_client.h"
#include "core/upload_manager.h"
#include "core/download_manager.h"
#include "core/chunk_processor.h"

#include <sstream>
#include <thread>
#include <unordered_map>

#pragma comment(lib, "comdlg32.lib")

MainWindow::MainWindow(HINSTANCE hInstance)
    : hInstance(hInstance), hwnd(nullptr), isConnected(false) {
    hListFiles = nullptr;
    hListNodes = nullptr;
    hBtnUpload = nullptr;
    hBtnDownload = nullptr;
    hBtnRefresh = nullptr;
    hBtnViewChunks = nullptr;
    hStatusBar = nullptr;
    hEditServer = nullptr;
    hEditPort = nullptr;
    hBtnConnect = nullptr;
    hLabelFiles = nullptr;
    hLabelNodes = nullptr;
}

MainWindow::~MainWindow() {
    DisconnectFromServer();
}

bool MainWindow::Create() {
    const char* className = "CourseStoreMainWindow";
    
    // Register window class
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = className;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    
    if (!RegisterClassEx(&wc)) {
        return false;
    }
    
    // Create window
    hwnd = CreateWindowEx(
        0,
        className,
        "CourseStore - Distributed File Storage",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1000, 700,
        nullptr,
        nullptr,
        hInstance,
        this
    );
    
    if (!hwnd) {
        return false;
    }
    
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    
    return true;
}

void MainWindow::Show(int nCmdShow) {
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    
    // Force layout update after showing window
    RECT rect;
    GetClientRect(hwnd, &rect);
    LayoutControls(rect.right - rect.left, rect.bottom - rect.top);
    InvalidateRect(hwnd, nullptr, TRUE);
    UpdateWindow(hwnd);
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    
    if (!pThis && uMsg == WM_CREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<MainWindow*>(pCreate->lpCreateParams);
        if (pThis) {
            // Set hwnd before setting USERDATA
            pThis->hwnd = hwnd;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        }
    }
    
    if (pThis) {
        switch (uMsg) {
            case WM_CREATE:
                // Ensure hwnd is set
                if (!pThis->hwnd) {
                    pThis->hwnd = hwnd;
                }
                pThis->OnCreate();
                return 0;
            case WM_COMMAND:
                pThis->OnCommand(wParam, lParam);
                return 0;
            case WM_SIZE:
                pThis->OnSize(LOWORD(lParam), HIWORD(lParam));
                return 0;
            case WM_PAINT: {
                PAINTSTRUCT ps;
                BeginPaint(hwnd, &ps);
                EndPaint(hwnd, &ps);
                return 0;
            }
            case WM_CLOSE:
                pThis->OnClose();
                return 0;
            case WM_DESTROY:
                pThis->OnDestroy();
                return 0;
            case WM_USER + 1: // Upload complete
                if (wParam == 1) {
                    pThis->UpdateStatus("File uploaded successfully");
                    pThis->RefreshFiles();
                    pThis->RefreshNodes();
                    MessageBox(pThis->hwnd, "File uploaded successfully!", "Success", MB_OK | MB_ICONINFORMATION);
                } else {
                    pThis->UpdateStatus("Upload failed");
                    MessageBox(pThis->hwnd, "Failed to upload file", "Error", MB_OK | MB_ICONERROR);
                }
                return 0;
            case WM_USER + 2: // Download complete
                if (wParam == 1) {
                    pThis->UpdateStatus("File downloaded successfully");
                    MessageBox(pThis->hwnd, "File downloaded successfully!", "Success", MB_OK | MB_ICONINFORMATION);
                } else {
                    pThis->UpdateStatus("Download failed");
                    MessageBox(pThis->hwnd, "Failed to download file", "Error", MB_OK | MB_ICONERROR);
                }
                return 0;
        }
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void MainWindow::OnCreate() {
    CreateControls();
    
    // Set default values
    if (hEditServer) {
        SetWindowText(hEditServer, "127.0.0.1");
    }
    if (hEditPort) {
        SetWindowText(hEditPort, "8080");
    }
    
    // Layout controls with initial window size
    RECT rect;
    GetClientRect(hwnd, &rect);
    LayoutControls(rect.right - rect.left, rect.bottom - rect.top);
    
    // Force redraw
    InvalidateRect(hwnd, nullptr, TRUE);
    UpdateWindow(hwnd);
    
    UpdateStatus("Disconnected. Click 'Connect' to connect to metadata server.");
}

void MainWindow::CreateControls() {
    // hwnd should be set by now (from WindowProc WM_CREATE handler)
    if (!hwnd) {
        // This should not happen, but if it does, try to get it from the window
        // Actually, we can't do that, so just use hInstance
        MessageBox(nullptr, "Window handle is null in CreateControls - using hInstance", "Warning", MB_OK | MB_ICONWARNING);
    }
    
    // Get the instance from the window or use stored hInstance
    HINSTANCE instance = hInstance;
    if (hwnd) {
        HINSTANCE windowInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
        if (windowInstance) {
            instance = windowInstance;
        }
    }
    
    int x = 10, y = 10;
    int height = 25;
    
    // Server connection group
    CreateWindow("STATIC", "Server:", WS_VISIBLE | WS_CHILD,
        x, y, 60, height, hwnd, nullptr, instance, nullptr);
    
    hEditServer = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
        x + 70, y, 120, height, hwnd, (HMENU)(INT_PTR)ID_EDIT_SERVER, instance, nullptr);
    if (!hEditServer) {
        DWORD error = GetLastError();
        char msg[256];
        sprintf_s(msg, "Failed to create server edit control. Error: %lu", error);
        MessageBox(hwnd, msg, "Error", MB_OK | MB_ICONERROR);
    }
    
    CreateWindow("STATIC", "Port:", WS_VISIBLE | WS_CHILD,
        x + 200, y, 40, height, hwnd, nullptr, instance, nullptr);
    
    hEditPort = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
        x + 250, y, 60, height, hwnd, (HMENU)(INT_PTR)ID_EDIT_PORT, instance, nullptr);
    if (!hEditPort) {
        DWORD error = GetLastError();
        char msg[256];
        sprintf_s(msg, "Failed to create port edit control. Error: %lu", error);
        MessageBox(hwnd, msg, "Error", MB_OK | MB_ICONERROR);
    }
    
    hBtnConnect = CreateWindow("BUTTON", "Connect", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        x + 320, y, 80, height, hwnd, (HMENU)(INT_PTR)ID_BTN_CONNECT, instance, nullptr);
    if (!hBtnConnect) {
        DWORD error = GetLastError();
        char msg[256];
        sprintf_s(msg, "Failed to create connect button. Error: %lu", error);
        MessageBox(hwnd, msg, "Error", MB_OK | MB_ICONERROR);
    }
    
    y = 50; // Position for lists (will be adjusted in LayoutControls)
    
    // File list label
    hLabelFiles = CreateWindow("STATIC", "Files:", WS_VISIBLE | WS_CHILD,
        x, y - 20, 100, height, hwnd, nullptr, instance, nullptr);
    
    // File list (will be positioned in LayoutControls)
    hListFiles = CreateWindowEx(WS_EX_CLIENTEDGE, "LISTBOX", "",
        WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_BORDER | LBS_NOTIFY,
        x, y, 200, 200, hwnd, (HMENU)(INT_PTR)ID_LIST_FILES, instance, nullptr);
    if (!hListFiles) {
        DWORD error = GetLastError();
        char msg[256];
        sprintf_s(msg, "Failed to create file list. Error: %lu", error);
        MessageBox(hwnd, msg, "Error", MB_OK | MB_ICONERROR);
    }
    
    // Node list label
    hLabelNodes = CreateWindow("STATIC", "Storage Nodes:", WS_VISIBLE | WS_CHILD,
        x + 250, y - 20, 150, height, hwnd, nullptr, instance, nullptr);
    
    // Node list (will be positioned in LayoutControls)
    hListNodes = CreateWindowEx(WS_EX_CLIENTEDGE, "LISTBOX", "",
        WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_BORDER,
        x + 250, y, 200, 200, hwnd, (HMENU)(INT_PTR)ID_LIST_NODES, instance, nullptr);
    if (!hListNodes) {
        DWORD error = GetLastError();
        char msg[256];
        sprintf_s(msg, "Failed to create node list. Error: %lu", error);
        MessageBox(hwnd, msg, "Error", MB_OK | MB_ICONERROR);
    }
    
    // Buttons (will be positioned in LayoutControls)
    hBtnUpload = CreateWindow("BUTTON", "Upload File", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        x, y + 220, 120, height, hwnd, (HMENU)(INT_PTR)ID_BTN_UPLOAD, instance, nullptr);
    
    hBtnDownload = CreateWindow("BUTTON", "Download File", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        x + 130, y + 220, 120, height, hwnd, (HMENU)(INT_PTR)ID_BTN_DOWNLOAD, instance, nullptr);
    
    hBtnViewChunks = CreateWindow("BUTTON", "View Chunks", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        x + 260, y + 220, 120, height, hwnd, (HMENU)(INT_PTR)ID_BTN_VIEW_CHUNKS, instance, nullptr);
    
    hBtnRefresh = CreateWindow("BUTTON", "Refresh", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        x + 390, y + 220, 100, height, hwnd, (HMENU)(INT_PTR)ID_BTN_REFRESH, instance, nullptr);
    
    // Status bar
    hStatusBar = CreateWindowEx(0, STATUSCLASSNAME, "",
        WS_VISIBLE | WS_CHILD | SBARS_SIZEGRIP,
        0, 0, 0, 0, hwnd, nullptr, instance, nullptr);
    if (!hStatusBar) {
        DWORD error = GetLastError();
        char msg[256];
        sprintf_s(msg, "Failed to create status bar. Error: %lu", error);
        MessageBox(hwnd, msg, "Error", MB_OK | MB_ICONERROR);
    }
    
    // Enable/disable buttons based on connection state
    EnableWindow(hBtnUpload, FALSE);
    EnableWindow(hBtnDownload, FALSE);
    EnableWindow(hBtnViewChunks, FALSE);
    EnableWindow(hBtnRefresh, FALSE);
    
    // Force update to make sure controls are visible
    UpdateWindow(hwnd);
    InvalidateRect(hwnd, nullptr, TRUE);
}

void MainWindow::OnCommand(WPARAM wParam, LPARAM lParam) {
    int id = LOWORD(wParam);
    
    switch (id) {
        case ID_BTN_CONNECT:
            if (isConnected) {
                DisconnectFromServer();
            } else {
                ConnectToServer();
            }
            break;
        case ID_BTN_UPLOAD:
            UploadFile();
            break;
        case ID_BTN_DOWNLOAD:
            DownloadFile();
            break;
        case ID_BTN_REFRESH:
            RefreshFiles();
            RefreshNodes();
            break;
        case ID_BTN_VIEW_CHUNKS:
            ViewChunks();
            break;
        case ID_LIST_FILES:
            if (HIWORD(wParam) == LBN_SELCHANGE) {
                selectedFile = GetSelectedFile();
            }
            break;
    }
}

void MainWindow::OnSize(int width, int height) {
    LayoutControls(width, height);
}

void MainWindow::LayoutControls(int width, int height) {
    if (width <= 0 || height <= 0) return;
    
    // Status bar at bottom
    if (hStatusBar) {
        SendMessage(hStatusBar, WM_SIZE, 0, 0);
        RECT statusRect = {};
        GetWindowRect(hStatusBar, &statusRect);
        height -= (statusRect.bottom - statusRect.top);
    }
    
    // Calculate positions
    int x = 10;
    int y = 50; // Start below connection controls
    int listHeight = height - 150;
    if (listHeight < 100) listHeight = 100; // Minimum height
    
    // Resize file list (left side)
    int leftListWidth = (width - 30) / 2; // Half width minus spacing
    if (leftListWidth < 200) leftListWidth = 200; // Minimum width
    
    // Move file list label
    if (hLabelFiles) {
        MoveWindow(hLabelFiles, x, y - 20, 100, 25, TRUE);
    }
    if (hListFiles) {
        MoveWindow(hListFiles, x, y, leftListWidth, listHeight, TRUE);
    }
    
    // Resize node list (right side)
    int rightListX = x + leftListWidth + 10;
    int rightListWidth = width - rightListX - 10;
    if (rightListWidth < 200) rightListWidth = 200; // Minimum width
    
    // Move node list label
    if (hLabelNodes) {
        MoveWindow(hLabelNodes, rightListX, y - 20, 150, 25, TRUE);
    }
    if (hListNodes) {
        MoveWindow(hListNodes, rightListX, y, rightListWidth, listHeight, TRUE);
    }
    
    // Move buttons
    int btnY = y + listHeight + 10;
    if (hBtnUpload) {
        MoveWindow(hBtnUpload, x, btnY, 120, 25, TRUE);
    }
    if (hBtnDownload) {
        MoveWindow(hBtnDownload, x + 130, btnY, 120, 25, TRUE);
    }
    if (hBtnViewChunks) {
        MoveWindow(hBtnViewChunks, x + 260, btnY, 120, 25, TRUE);
    }
    if (hBtnRefresh) {
        MoveWindow(hBtnRefresh, x + 390, btnY, 100, 25, TRUE);
    }
    
    // Force redraw
    InvalidateRect(hwnd, nullptr, TRUE);
}

void MainWindow::OnClose() {
    if (isConnected) {
        int result = MessageBox(hwnd,
            "Are you sure you want to disconnect and exit?",
            "Confirm Exit",
            MB_YESNO | MB_ICONQUESTION);
        
        if (result == IDYES) {
            DestroyWindow(hwnd);
        }
    } else {
        DestroyWindow(hwnd);
    }
}

void MainWindow::OnDestroy() {
    DisconnectFromServer();
    PostQuitMessage(0);
}

void MainWindow::ConnectToServer() {
    char server[256] = {0};
    char port[16] = {0};
    
    GetWindowText(hEditServer, server, sizeof(server));
    GetWindowText(hEditPort, port, sizeof(port));
    
    if (strlen(server) == 0 || strlen(port) == 0) {
        ShowMessage("Error", "Please enter server address and port", MB_OK | MB_ICONERROR);
        return;
    }
    
    int portNum = atoi(port);
    if (portNum <= 0 || portNum > 65535) {
        ShowMessage("Error", "Invalid port number", MB_OK | MB_ICONERROR);
        return;
    }
    
    UpdateStatus("Connecting to server...");
    
    // Create client components
    metadataClient = std::make_unique<MetadataClient>(server, portNum);
    
    if (!metadataClient->TestConnection()) {
        ShowMessage("Error", "Failed to connect to metadata server", MB_OK | MB_ICONERROR);
        UpdateStatus("Connection failed");
        metadataClient.reset();
        return;
    }
    
    uploadManager = std::make_unique<UploadManager>(metadataClient.get());
    downloadManager = std::make_unique<DownloadManager>(metadataClient.get());
    
    isConnected = true;
    SetWindowText(hBtnConnect, "Disconnect");
    EnableWindow(hEditServer, FALSE);
    EnableWindow(hEditPort, FALSE);
    EnableWindow(hBtnUpload, TRUE);
    EnableWindow(hBtnDownload, TRUE);
    EnableWindow(hBtnViewChunks, TRUE);
    EnableWindow(hBtnRefresh, TRUE);
    
    UpdateStatus("Connected to server");
    
    // Load initial data
    RefreshFiles();
    RefreshNodes();
}

void MainWindow::DisconnectFromServer() {
    if (!isConnected) return;
    
    downloadManager.reset();
    uploadManager.reset();
    metadataClient.reset();
    
    isConnected = false;
    SetWindowText(hBtnConnect, "Connect");
    EnableWindow(hEditServer, TRUE);
    EnableWindow(hEditPort, TRUE);
    EnableWindow(hBtnUpload, FALSE);
    EnableWindow(hBtnDownload, FALSE);
    EnableWindow(hBtnViewChunks, FALSE);
    EnableWindow(hBtnRefresh, FALSE);
    
    // Clear lists
    SendMessage(hListFiles, LB_RESETCONTENT, 0, 0);
    SendMessage(hListNodes, LB_RESETCONTENT, 0, 0);
    files.clear();
    nodes.clear();
    
    UpdateStatus("Disconnected");
}

void MainWindow::RefreshFiles() {
    if (!isConnected) return;
    
    UpdateStatus("Refreshing file list...");
    
    SendMessage(hListFiles, LB_RESETCONTENT, 0, 0);
    files.clear();
    
    auto fileList = metadataClient->ListFiles();
    
    for (const auto& file : fileList) {
        FileInfo info;
        info.filename = file.first;
        info.size = file.second;
        
        // Get chunk count from metadata
        auto metadata = metadataClient->RequestDownload(file.first);
        info.chunkCount = metadata.chunks.size();
        
        files.push_back(info);
        
        // Format: "filename (size bytes, N chunks)"
        std::stringstream ss;
        ss << info.filename << " (" << info.size << " bytes, " << info.chunkCount << " chunks)";
        SendMessage(hListFiles, LB_ADDSTRING, 0, (LPARAM)ss.str().c_str());
    }
    
    UpdateStatus("File list refreshed");
}

void MainWindow::RefreshNodes() {
    if (!isConnected) return;
    
    UpdateStatus("Refreshing node list...");
    
    SendMessage(hListNodes, LB_RESETCONTENT, 0, 0);
    nodes.clear();
    
    // Get nodes from metadata server
    auto nodeList = metadataClient->ListNodes();
    
    for (const auto& nodeInfo : nodeList) {
        NodeInfo node;
        node.nodeId = nodeInfo.nodeId;
        node.ipAddress = nodeInfo.ipAddress;
        node.port = nodeInfo.port;
        node.freeSpace = nodeInfo.freeSpace;
        node.isActive = true; // Nodes from LIST_NODES are active
        
        nodes.push_back(node);
        
        // Format: "nodeId (ip:port) - Free: X bytes - Active/Inactive"
        std::stringstream ss;
        ss << node.nodeId;
        if (!node.ipAddress.empty()) {
            ss << " (" << node.ipAddress << ":" << node.port << ")";
        }
        ss << " - Free: " << (node.freeSpace / 1024 / 1024) << " MB";
        ss << " - " << (node.isActive ? "Active" : "Inactive");
        
        SendMessage(hListNodes, LB_ADDSTRING, 0, (LPARAM)ss.str().c_str());
    }
    
    UpdateStatus("Node list refreshed - " + std::to_string(nodes.size()) + " nodes");
}

void MainWindow::UploadFile() {
    if (!isConnected) return;
    
    std::string filepath = ShowOpenFileDialog();
    if (filepath.empty()) return;
    
    // Extract filename from path
    size_t pos = filepath.find_last_of("\\/");
    std::string filename = (pos != std::string::npos) ? filepath.substr(pos + 1) : filepath;
    
    UpdateStatus("Uploading file: " + filename);
    
    // Run upload in separate thread to avoid blocking UI
    std::thread([this, filepath, filename]() {
        bool success = uploadManager->UploadFile(filepath, filename);
        
        PostMessage(hwnd, WM_USER + 1, success ? 1 : 0, 0);
    }).detach();
}

void MainWindow::DownloadFile() {
    if (!isConnected) return;
    
    std::string filename = GetSelectedFile();
    if (filename.empty()) {
        ShowMessage("Error", "Please select a file to download", MB_OK | MB_ICONERROR);
        return;
    }
    
    std::string savepath = ShowSaveFileDialog(filename);
    if (savepath.empty()) return;
    
    UpdateStatus("Downloading file: " + filename);
    
    // Run download in separate thread
    std::thread([this, filename, savepath]() {
        bool success = downloadManager->DownloadFile(filename, savepath);
        
        PostMessage(hwnd, WM_USER + 2, success ? 1 : 0, 0);
    }).detach();
}

void MainWindow::ViewChunks() {
    std::string filename = GetSelectedFile();
    if (filename.empty()) {
        ShowMessage("Error", "Please select a file to view chunks", MB_OK | MB_ICONERROR);
        return;
    }
    
    ShowChunkLocations(filename);
}

void MainWindow::ShowChunkLocations(const std::string& filename) {
    if (!isConnected) return;
    
    auto metadata = metadataClient->RequestDownload(filename);
    
    std::vector<ChunkInfo> chunks;
    for (const auto& chunk : metadata.chunks) {
        ChunkInfo info;
        info.chunkId = chunk.chunkId;
        info.index = chunk.index;
        info.size = chunk.size;
        info.nodeIds = chunk.nodeIds;
        
        // Get node IPs and ports
        for (const auto& nodeId : chunk.nodeIds) {
            StorageNodeInfo nodeInfo;
            if (metadataClient->GetNodeInfo(nodeId, nodeInfo)) {
                info.nodeIps.push_back(nodeInfo.ipAddress);
                info.nodePorts.push_back(nodeInfo.port);
            } else {
                info.nodeIps.push_back("");
                info.nodePorts.push_back(0);
            }
        }
        
        chunks.push_back(info);
    }
    
    // Create and show chunk viewer window
    static std::unique_ptr<ChunkViewer> chunkViewer;
    chunkViewer = std::make_unique<ChunkViewer>(hInstance, hwnd);
    
    if (chunkViewer->Create(filename, chunks)) {
        chunkViewer->Show();
    } else {
        // Fallback to message box if window creation fails
        std::stringstream ss;
        ss << "File: " << filename << "\n\n";
        ss << "Total chunks: " << chunks.size() << "\n\n";
        
        for (const auto& chunk : chunks) {
            ss << "Chunk " << chunk.index << ":\n";
            ss << "  ID: " << chunk.chunkId.substr(0, 16) << "...\n";
            ss << "  Size: " << chunk.size << " bytes\n";
            ss << "  Nodes: ";
            for (size_t i = 0; i < chunk.nodeIds.size(); ++i) {
                ss << chunk.nodeIds[i];
                if (i < chunk.nodeIps.size() && !chunk.nodeIps[i].empty()) {
                    ss << " (" << chunk.nodeIps[i];
                    if (i < chunk.nodePorts.size()) {
                        ss << ":" << chunk.nodePorts[i];
                    }
                    ss << ")";
                }
                if (i < chunk.nodeIds.size() - 1) ss << ", ";
            }
            ss << "\n\n";
        }
        
        ShowMessage("Chunk Locations", ss.str(), MB_OK);
    }
}

void MainWindow::UpdateFileList() {
    RefreshFiles();
}

void MainWindow::UpdateNodeList() {
    RefreshNodes();
}

void MainWindow::UpdateStatus(const std::string& message) {
    SendMessage(hStatusBar, SB_SETTEXT, 0, (LPARAM)message.c_str());
}

std::string MainWindow::GetSelectedFile() {
    int index = SendMessage(hListFiles, LB_GETCURSEL, 0, 0);
    if (index == LB_ERR) return "";
    
    if (index < (int)files.size()) {
        return files[index].filename;
    }
    return "";
}

void MainWindow::ShowMessage(const std::string& title, const std::string& message, UINT type) {
    MessageBox(hwnd, message.c_str(), title.c_str(), type);
}

std::string MainWindow::ShowOpenFileDialog() {
    OPENFILENAME ofn = {};
    char szFile[260] = {0};
    
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    
    if (::GetOpenFileNameA(&ofn)) {
        return std::string(szFile);
    }
    
    return "";
}

std::string MainWindow::ShowSaveFileDialog(const std::string& defaultName) {
    OPENFILENAME ofn = {};
    char szFile[260] = {0};
    strncpy_s(szFile, defaultName.c_str(), sizeof(szFile) - 1);
    
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    
    if (::GetSaveFileNameA(&ofn)) {
        return std::string(szFile);
    }
    
    return "";
}

