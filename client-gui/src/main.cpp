#include "main_window.h"

#pragma comment(lib, "comctl32.lib")

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);
    
    // Initialize network (Winsock)
    #ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        MessageBox(nullptr, "Failed to initialize Winsock", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    #endif
    
    // Create main window
    MainWindow mainWindow(hInstance);
    
    if (!mainWindow.Create()) {
        MessageBox(nullptr, "Failed to create main window", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    mainWindow.Show(nCmdShow);
    
    // Message loop
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // Cleanup Winsock
    #ifdef _WIN32
    WSACleanup();
    #endif
    
    return 0;
}


