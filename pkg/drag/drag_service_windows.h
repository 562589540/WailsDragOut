#ifndef DRAG_SERVICE_H
#define DRAG_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <ole2.h>
#include <objbase.h>

// 自定义消息
#define WM_DRAG_REQUEST (WM_USER + 100)

// 全局唯一的拖拽服务实例
typedef struct {
    char pendingFilePath[MAX_PATH];
    HANDLE dragCompleteEvent;
    int dragResult;
    DWORD mainThreadId;     // 存储主线程ID
    HANDLE mainProcessHandle; // 存储主进程句柄
} DragService;

// C-API for Go
int startDragService();
void stopDragService();
int isDragAvailable();
int triggerDrag(const char* filePath);
// 新增：设置主线程信息的函数
void setMainThreadInfo(DWORD threadId, HANDLE processHandle);

#ifdef __cplusplus
}
#endif

#endif // DRAG_SERVICE_WINDOWS_H
