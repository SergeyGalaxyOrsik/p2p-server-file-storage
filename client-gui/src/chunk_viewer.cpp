#include "chunk_viewer.h"

#include <sstream>

ChunkViewer::ChunkViewer(HINSTANCE hInstance, HWND hParent)
    : hInstance(hInstance), hParent(hParent), hwnd(nullptr) {
    hListChunks = nullptr;
    hListNodes = nullptr;
    hBtnClose = nullptr;
}

ChunkViewer::~ChunkViewer() {
    if (hwnd) {
        DestroyWindow(hwnd);
    }
}

bool ChunkViewer::Create(const std::string& filename, const std::vector<ChunkInfo>& chunks) {
    this->filename = filename;
    this->chunks = chunks;
    
    const char* className = "CourseStoreChunkViewer";
    
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
    
    RegisterClassEx(&wc);
    
    // Create window
    hwnd = CreateWindowEx(
        WS_EX_DLGMODALFRAME,
        className,
        ("Chunk Locations - " + filename).c_str(),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        hParent,
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

void ChunkViewer::Show() {
    if (hwnd) {
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }
}

void ChunkViewer::Hide() {
    if (hwnd) {
        ShowWindow(hwnd, SW_HIDE);
    }
}

bool ChunkViewer::IsVisible() const {
    if (!hwnd) return false;
    return IsWindowVisible(hwnd) != FALSE;
}

LRESULT CALLBACK ChunkViewer::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    ChunkViewer* pThis = reinterpret_cast<ChunkViewer*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    
    if (!pThis && uMsg == WM_CREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<ChunkViewer*>(pCreate->lpCreateParams);
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
            case WM_CLOSE:
                pThis->OnClose();
                return 0;
        }
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void ChunkViewer::OnCreate() {
    CreateControls();
    UpdateChunkList();
}

void ChunkViewer::CreateControls() {
    if (!hwnd) {
        return;
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
    int width = 380, height = 25;
    
    // Chunk list
    CreateWindow("STATIC", "Chunks:", WS_VISIBLE | WS_CHILD,
        x, y, 100, height, hwnd, nullptr, instance, nullptr);
    
    y += 30;
    
    hListChunks = CreateWindowEx(WS_EX_CLIENTEDGE, "LISTBOX", "",
        WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_BORDER | LBS_NOTIFY,
        x, y, width, 450, hwnd, (HMENU)(INT_PTR)ID_LIST_CHUNKS, instance, nullptr);
    
    // Node list
    int x2 = x + width + 10;
    CreateWindow("STATIC", "Nodes for selected chunk:", WS_VISIBLE | WS_CHILD,
        x2, y - 30, 200, height, hwnd, nullptr, instance, nullptr);
    
    hListNodes = CreateWindowEx(WS_EX_CLIENTEDGE, "LISTBOX", "",
        WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_BORDER,
        x2, y, width, 450, hwnd, (HMENU)(INT_PTR)ID_LIST_NODES, instance, nullptr);
    
    y += 460;
    
    // Close button
    hBtnClose = CreateWindow("BUTTON", "Close", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        x, y, 100, height, hwnd, (HMENU)(INT_PTR)ID_BTN_CLOSE, instance, nullptr);
    
    // Force update
    UpdateWindow(hwnd);
    InvalidateRect(hwnd, nullptr, TRUE);
}

void ChunkViewer::OnCommand(WPARAM wParam, LPARAM lParam) {
    int id = LOWORD(wParam);
    
    switch (id) {
        case ID_BTN_CLOSE:
            OnClose();
            break;
        case ID_LIST_CHUNKS:
            if (HIWORD(wParam) == LBN_SELCHANGE) {
                int index = SendMessage(hListChunks, LB_GETCURSEL, 0, 0);
                if (index != LB_ERR && index < (int)chunks.size()) {
                    OnChunkSelected(index);
                }
            }
            break;
    }
}

void ChunkViewer::OnSize(int width, int height) {
    LayoutControls(width, height);
}

void ChunkViewer::LayoutControls(int width, int height) {
    int listHeight = height - 100;
    int listWidth = (width - 30) / 2;
    
    MoveWindow(hListChunks, 10, 40, listWidth, listHeight, TRUE);
    MoveWindow(hListNodes, 20 + listWidth, 40, listWidth, listHeight, TRUE);
    
    MoveWindow(hBtnClose, 10, listHeight + 50, 100, 25, TRUE);
}

void ChunkViewer::OnClose() {
    Hide();
}

void ChunkViewer::UpdateChunkList() {
    if (!hListChunks) {
        return;
    }
    
    SendMessage(hListChunks, LB_RESETCONTENT, 0, 0);
    
    if (chunks.empty()) {
        SendMessage(hListChunks, LB_ADDSTRING, 0, (LPARAM)"No chunks available");
        return;
    }
    
    for (const auto& chunk : chunks) {
        std::stringstream ss;
        ss << "Chunk " << chunk.index << " - " << (chunk.size / 1024) << " KB - " 
           << chunk.nodeIds.size() << " nodes";
        std::string str = ss.str();
        SendMessage(hListChunks, LB_ADDSTRING, 0, (LPARAM)str.c_str());
    }
    
    // Force redraw
    InvalidateRect(hListChunks, nullptr, TRUE);
    UpdateWindow(hListChunks);
}

void ChunkViewer::OnChunkSelected(int index) {
    if (index < 0 || index >= (int)chunks.size()) return;
    
    UpdateNodeList(chunks[index]);
}

void ChunkViewer::UpdateNodeList(const ChunkInfo& chunk) {
    if (!hListNodes) {
        return;
    }
    
    SendMessage(hListNodes, LB_RESETCONTENT, 0, 0);
    
    if (chunk.nodeIds.empty()) {
        SendMessage(hListNodes, LB_ADDSTRING, 0, (LPARAM)"No nodes available");
        return;
    }
    
    for (size_t i = 0; i < chunk.nodeIds.size(); ++i) {
        std::stringstream ss;
        ss << "Node: " << chunk.nodeIds[i];
        if (i < chunk.nodeIps.size() && !chunk.nodeIps[i].empty()) {
            ss << " (" << chunk.nodeIps[i];
            if (i < chunk.nodePorts.size() && chunk.nodePorts[i] > 0) {
                ss << ":" << chunk.nodePorts[i];
            }
            ss << ")";
        }
        std::string str = ss.str();
        SendMessage(hListNodes, LB_ADDSTRING, 0, (LPARAM)str.c_str());
    }
    
    // Force redraw
    InvalidateRect(hListNodes, nullptr, TRUE);
    UpdateWindow(hListNodes);
}


