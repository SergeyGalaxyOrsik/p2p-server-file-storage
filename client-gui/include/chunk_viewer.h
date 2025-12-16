#pragma once

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <commctrl.h>

#include "chunk_info.h"

#include <string>
#include <vector>

class ChunkViewer {
private:
    HWND hwnd;
    HINSTANCE hInstance;
    HWND hParent;
    
    HWND hListChunks;
    HWND hListNodes;
    HWND hBtnClose;
    
    std::vector<ChunkInfo> chunks;
    std::string filename;
    
    static const int ID_LIST_CHUNKS = 2001;
    static const int ID_LIST_NODES = 2002;
    static const int ID_BTN_CLOSE = 2003;
    
public:
    ChunkViewer(HINSTANCE hInstance, HWND hParent);
    ~ChunkViewer();
    
    bool Create(const std::string& filename, const std::vector<ChunkInfo>& chunks);
    void Show();
    void Hide();
    bool IsVisible() const;
    
    void UpdateChunkList();
    void OnChunkSelected(int index);
    void UpdateNodeList(const ChunkInfo& chunk);
    
    // Message handlers
    void OnCreate();
    void OnCommand(WPARAM wParam, LPARAM lParam);
    void OnSize(int width, int height);
    void OnClose();
    
private:
    void CreateControls();
    void LayoutControls(int width, int height);
    
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

