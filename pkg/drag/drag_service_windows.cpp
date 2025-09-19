// Target Windows 7 or later to get access to modern shell APIs like SHCreateFileDataObject
#define _WIN32_WINNT 0x0601

#include "drag_service_windows.h"
#include <stdio.h>
#include <wchar.h>
#include <ole2.h>
#include <shlobj.h>
#include <gdiplus.h>
#include <shobjidl.h> // For IDragSourceHelper

using namespace Gdiplus;

// 全局拖拽服务实例 - 存储拖拽相关信息
static DragService g_dragService = {0};

// C++ 风格的 IDropSource 实现
class ServiceDropSource : public IDropSource {
public:
    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) {
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDropSource)) {
            *ppvObject = static_cast<IDropSource*>(this);
            AddRef();
            return S_OK;
        }
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() {
        return InterlockedIncrement(&m_refCount);
    }

    STDMETHODIMP_(ULONG) Release() {
        ULONG count = InterlockedDecrement(&m_refCount);
        if (count == 0) {
            delete this;
        }
        return count;
    }

    // IDropSource methods
    STDMETHODIMP QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) {
        if (fEscapePressed) {
            printf("SERVICE: Drag cancelled by user (ESC pressed)\n");
            return DRAGDROP_S_CANCEL;
        }
        
        // 检测是否要放置拖拽项
        if (!(grfKeyState & MK_LBUTTON)) {
            // 鼠标按键释放，检查目标窗口
            POINT pt;
            GetCursorPos(&pt);
            HWND targetWindow = WindowFromPoint(pt);
            
            if (targetWindow) {
                // 获取自己的进程ID
                DWORD selfProcessId = GetCurrentProcessId();
                
                // 向上查找窗口层次，检查是否有任何父窗口属于自己的进程
                HWND checkWindow = targetWindow;
                bool isSelfApplication = false;
                
                while (checkWindow) {
                    DWORD targetProcessId;
                    GetWindowThreadProcessId(checkWindow, &targetProcessId);
                    
                    printf("SERVICE: Checking window %p - PID: %lu (Self: %lu)\n", 
                           checkWindow, targetProcessId, selfProcessId);
                    
                    if (targetProcessId == selfProcessId) {
                        isSelfApplication = true;
                        printf("SERVICE: ⚠️  Found self-owned window in hierarchy: %p\n", checkWindow);
                        break;
                    }
                    
                    // 检查父窗口
                    HWND parentWindow = GetParent(checkWindow);
                    if (!parentWindow) {
                        // 如果没有父窗口，检查所有者窗口
                        parentWindow = GetWindow(checkWindow, GW_OWNER);
                    }
                    checkWindow = parentWindow;
                }
                
                printf("SERVICE: Final drop target check - Target HWND: %p, Is self app: %s\n", 
                       targetWindow, isSelfApplication ? "YES" : "NO");
                
                // 如果找到任何属于自己进程的窗口，取消拖拽
                if (isSelfApplication) {
                    printf("SERVICE: ⚠️  DROP TARGET IS SELF APPLICATION - CANCELLING drag to prevent self-drop\n");
                    return DRAGDROP_S_CANCEL;
                }
                
                printf("SERVICE: Drop target is external application - allowing drop\n");
            }
            
            return DRAGDROP_S_DROP;
        }
        return S_OK;
    }

    STDMETHODIMP GiveFeedback(DWORD dwEffect) {
        return DRAGDROP_S_USEDEFAULTCURSORS;
    }

    ServiceDropSource() : m_refCount(1) {}

private:
    LONG m_refCount;
};

// 简化版窗口过程，只提供一个有效的 HWND
LRESULT CALLBACK DragServiceWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


// 拖拽服务线程函数
DWORD WINAPI DragServiceThreadProc(LPVOID lpParam) {
    printf("SERVICE THREAD: Starting on-demand execution...\n");
    
    // RAII-style COM management
    struct ComInitializer {
        ComInitializer() { OleInitialize(NULL); }
        ~ComInitializer() { OleUninitialize(); }
    } comInit;

    // RAII-style Window Class management
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = DragServiceWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"WailsOnDemandDragService";
    if (!RegisterClassW(&wc)) {
        SetEvent(g_dragService.dragCompleteEvent);
        return 1;
    }
    struct ClassUnregisterer {
        ~ClassUnregisterer() { UnregisterClassW(L"WailsOnDemandDragService", GetModuleHandle(NULL)); }
    } classUnreg;

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Drag Helper", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, wc.hInstance, NULL);
    if (!hwnd) {
        SetEvent(g_dragService.dragCompleteEvent);
        return 1;
    }

    // --- Core Drag Logic ---
    g_dragService.dragResult = 0;
    printf("SERVICE: Processing drag request for: %s\n", g_dragService.pendingFilePath);
            
    // 🚀 NEW: 多重策略查找主窗口
    HWND mainWnd = NULL;
    
    // 策略1: 尝试获取当前前台窗口
    mainWnd = GetForegroundWindow();
    if (mainWnd && IsWindowVisible(mainWnd)) {
        printf("SERVICE: Found main window using GetForegroundWindow: %p\n", mainWnd);
    } else {
        printf("SERVICE: GetForegroundWindow failed or returned invisible window: %p\n", mainWnd);
        mainWnd = NULL;
    }
    
    // 策略2: 如果前台窗口方法失败，使用存储的线程ID枚举
    if (!mainWnd && g_dragService.mainThreadId != 0) {
        printf("SERVICE: Falling back to thread ID enumeration with stored ID: %lu\n", g_dragService.mainThreadId);
        
        // 枚举所有窗口找到属于主线程的可见窗口，优先查找主窗口
        struct WindowEnumData {
            DWORD targetThreadId;
            HWND foundWindow;
            HWND bestWindow;
            int totalFound;
        } enumData = { g_dragService.mainThreadId, NULL, NULL, 0 };
        
        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            WindowEnumData* data = (WindowEnumData*)lParam;
            DWORD windowThreadId = GetWindowThreadProcessId(hwnd, NULL);
            
            if (windowThreadId == data->targetThreadId && IsWindowVisible(hwnd)) {
                data->totalFound++;
                printf("SERVICE: Found window %p (thread %lu)\n", hwnd, windowThreadId);
                
                // 检查是否是主窗口（有标题栏，不是工具窗口）
                LONG style = GetWindowLong(hwnd, GWL_STYLE);
                LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
                
                if ((style & WS_CAPTION) && !(exStyle & WS_EX_TOOLWINDOW)) {
                    // 这是一个主窗口，优先选择
                    data->bestWindow = hwnd;
                    printf("SERVICE: This is a main window (has caption, not tool window)\n");
                    return FALSE; // 停止枚举，找到最佳匹配
                } else if (!data->foundWindow) {
                    // 如果还没找到任何窗口，记录这个作为备选
                    data->foundWindow = hwnd;
                    printf("SERVICE: This is a fallback window\n");
                }
            }
            return TRUE; // 继续枚举
        }, (LPARAM)&enumData);
        
        // 优先使用最佳窗口，否则使用找到的第一个窗口
        mainWnd = enumData.bestWindow ? enumData.bestWindow : enumData.foundWindow;
        
        printf("SERVICE: Thread enumeration results - Total: %d, BestWindow: %p, FoundWindow: %p, Selected: %p\n", 
               enumData.totalFound, enumData.bestWindow, enumData.foundWindow, mainWnd);
    }
    
    // 策略3: 最后尝试获取桌面窗口的子窗口
    if (!mainWnd) {
        printf("SERVICE: Last resort - trying to find any visible top-level window\n");
        mainWnd = FindWindowExA(NULL, NULL, NULL, NULL);
        while (mainWnd && !IsWindowVisible(mainWnd)) {
            mainWnd = FindWindowExA(NULL, mainWnd, NULL, NULL);
        }
        if (mainWnd) {
            printf("SERVICE: Found fallback window: %p\n", mainWnd);
        }
    }

    if (mainWnd) {
        printf("SERVICE: Using main window HWND: %p\n", mainWnd);
        
        // 获取窗口的实际线程ID
        DWORD windowThreadId = GetWindowThreadProcessId(mainWnd, NULL);
        DWORD selfThreadId = GetCurrentThreadId();
        BOOL attached = FALSE;
        
        printf("SERVICE: Window thread ID: %lu, Self thread ID: %lu\n", windowThreadId, selfThreadId);
        
        if (windowThreadId != selfThreadId) {
            attached = AttachThreadInput(selfThreadId, windowThreadId, TRUE);
            printf("SERVICE: AttachThreadInput result: %s\n", attached ? "SUCCESS" : "FAILED");
        } else {
            printf("SERVICE: Same thread, no need to attach\n");
        }

        wchar_t wPath[MAX_PATH];
        if (MultiByteToWideChar(CP_UTF8, 0, g_dragService.pendingFilePath, -1, wPath, MAX_PATH) > 0) {
            // 🚀 最终方案: 彻底抛弃自定义 CDataObject，使用 Shell API 创建功能完整的原生 IDataObject
            IDataObject* pDataObject = NULL;
            PIDLIST_ABSOLUTE pidl;
            if (SUCCEEDED(SHParseDisplayName(wPath, NULL, &pidl, 0, NULL))) {
                IShellItem* pShellItem;
                if (SUCCEEDED(SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&pShellItem)))) {
                    pShellItem->BindToHandler(NULL, BHID_DataObject, IID_PPV_ARGS(&pDataObject));
                    pShellItem->Release();
                }
                CoTaskMemFree(pidl);
            }
            
            if (pDataObject) {
                IDropSource* pDropSource = new ServiceDropSource();
                if (pDropSource) {
                    // 🚀 最终修正: 使用更可靠的方法将 HICON 转换为带 Alpha 通道的 32 位 HBITMAP，确保预览图正确显示
                    IDragSourceHelper* pDragSourceHelper = NULL;
                    HBITMAP hbmpDragImage = NULL; // 用于拖拽预览的最终 32 位位图
                    
                    CoCreateInstance(CLSID_DragDropHelper, NULL, CLSCTX_INPROC_SERVER, IID_IDragSourceHelper, (void**)&pDragSourceHelper);

                    if (pDragSourceHelper) {
                        SHFILEINFOW sfi = {0};
                        HICON hIcon = NULL;

                        if (SHGetFileInfoW(wPath, FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES)) {
                            hIcon = sfi.hIcon;
                        }

                        if (hIcon) {
                            ICONINFO iconInfo = {0};
                            if (GetIconInfo(hIcon, &iconInfo)) {
                                BITMAP bmp = {0};
                                if (GetObject(iconInfo.hbmColor, sizeof(bmp), &bmp)) {
                                    HDC hdcScreen = GetDC(NULL);
                                    HDC hdcMem = CreateCompatibleDC(hdcScreen);
                                    
                                    BITMAPINFO bi = {0};
                                    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                                    bi.bmiHeader.biWidth = bmp.bmWidth;
                                    bi.bmiHeader.biHeight = -bmp.bmHeight; // top-down DIB
                                    bi.bmiHeader.biPlanes = 1;
                                    bi.bmiHeader.biBitCount = 32;
                                    bi.bmiHeader.biCompression = BI_RGB;

                                    void* pBits;
                                    hbmpDragImage = CreateDIBSection(hdcScreen, &bi, DIB_RGB_COLORS, &pBits, NULL, 0);

                                    if (hbmpDragImage) {
                                        HGDIOBJ hbmOld = SelectObject(hdcMem, hbmpDragImage);
                                        // DrawIconEx会正确处理透明度，将图标绘制到我们的32位DIB上
                                        DrawIconEx(hdcMem, 0, 0, hIcon, bmp.bmWidth, bmp.bmHeight, 0, NULL, DI_NORMAL);
                                        SelectObject(hdcMem, hbmOld);

                                        SHDRAGIMAGE shdi;
                                        shdi.sizeDragImage.cx = bmp.bmWidth;
                                        shdi.sizeDragImage.cy = bmp.bmHeight;
                                        shdi.hbmpDragImage = hbmpDragImage;
                                        shdi.crColorKey = CLR_NONE; // 使用Alpha通道，不需要颜色键
                                        shdi.ptOffset.x = bmp.bmWidth / 2; // 将光标置于预览图中心
                                        shdi.ptOffset.y = bmp.bmHeight / 2;
                                        
                                        pDragSourceHelper->InitializeFromBitmap(&shdi, pDataObject);
                                    }
                                    DeleteDC(hdcMem);
                                    ReleaseDC(NULL, hdcScreen);
                                }
                                DeleteObject(iconInfo.hbmColor);
                                DeleteObject(iconInfo.hbmMask);
                            }
                            DestroyIcon(hIcon);
                        }
                    }
                    
                    // 🚀 最终修正: 强行将主窗口设为前景、激活并拥有焦点。
                    // 这是让操作系统完全相信拖拽操作是合法源于我们应用的关键一步，
                    // 仅有 AttachThreadInput 在某些情况下是不够的。
                    SetForegroundWindow(mainWnd);
                    SetActiveWindow(mainWnd);
                    SetFocus(mainWnd);
                    Sleep(100); // 给予系统一点时间来处理焦点变化
                    
                    DWORD dwEffect = DROPEFFECT_NONE;
                    HRESULT hr = DoDragDrop(pDataObject, pDropSource, DROPEFFECT_COPY | DROPEFFECT_MOVE, &dwEffect);
                    
                    // 🚀 NEW: 详细记录 DoDragDrop 的结果
                    printf("SERVICE: DoDragDrop completed. HRESULT: 0x%08lx, dwEffect: 0x%08lx\n", hr, dwEffect);

                    if (SUCCEEDED(hr)) {
                        if (hr == DRAGDROP_S_CANCEL) {
                            printf("SERVICE INFO: Drag was cancelled (possibly due to self-drop detection or user cancellation)\n");
                        } else if (dwEffect == DROPEFFECT_NONE) {
                            printf("SERVICE INFO: Drag was completed but dropped on an invalid target\n");
                        } else {
                            printf("SERVICE SUCCESS: Drag completed with effect: 0x%08lx\n", dwEffect);
                            g_dragService.dragResult = 1;
                        }
                    } else {
                        printf("SERVICE ERROR: DoDragDrop failed with HRESULT: 0x%08lx\n", hr);
                    }
                    
                    if (pDragSourceHelper) pDragSourceHelper->Release();
                    if (hbmpDragImage) DeleteObject(hbmpDragImage); // 清理我们创建的 DIB
                }
                pDropSource->Release();
                pDataObject->Release(); // 释放 Shell 创建的 COM 对象
            }
        }

        if (attached) {
            AttachThreadInput(selfThreadId, windowThreadId, FALSE);
            printf("SERVICE: DetachThreadInput completed\n");
        }
    } else {
        printf("SERVICE ERROR: Failed to find main window via thread ID.\n");
    }
    
    DestroyWindow(hwnd);
    MSG msg;
    while(PeekMessage(&msg, hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    printf("SERVICE THREAD: Execution finished.\n");
    SetEvent(g_dragService.dragCompleteEvent);
    return 0;
}


// 这个文件中的所有函数现在都在调用者（Go）的线程上运行
// 我们不再需要手动创建或管理线程

int startDragService() {
    return 1;
}
void stopDragService() {
    // 无操作
}
int isDragAvailable() {
    return 1;
}

// 设置主线程信息
void setMainThreadInfo(DWORD threadId, HANDLE processHandle) {
    g_dragService.mainThreadId = threadId;
    g_dragService.mainProcessHandle = processHandle;
    printf("SERVICE: Main thread info set - ThreadID: %lu, ProcessHandle: %p\n", threadId, processHandle);
}

// 触发拖拽操作
int triggerDrag(const char* filePath) {
    printf("TRIGGER: Starting on-demand drag thread for: %s\n", filePath);

    strncpy(g_dragService.pendingFilePath, filePath, MAX_PATH - 1);
    g_dragService.pendingFilePath[MAX_PATH - 1] = '\0';
    
    g_dragService.dragCompleteEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!g_dragService.dragCompleteEvent) return 0;

    HANDLE hThread = CreateThread(NULL, 0, DragServiceThreadProc, NULL, 0, NULL);
    if (!hThread) {
        CloseHandle(g_dragService.dragCompleteEvent);
        return 0;
    }
    
    WaitForSingleObject(g_dragService.dragCompleteEvent, INFINITE);
    
    int result = g_dragService.dragResult;
    CloseHandle(g_dragService.dragCompleteEvent);
    CloseHandle(hThread);

    return result;
}
