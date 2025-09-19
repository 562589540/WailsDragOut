//go:build windows
// +build windows

package drag

/*
#cgo CFLAGS: -I.
#cgo LDFLAGS: -luser32 -lkernel32 -lshell32 -lole32 -luuid -lgdiplus -lgdi32 -static

#include "drag_service_windows.h"
*/
import "C"
import (
	"fmt"
	"unsafe"

	"golang.org/x/sys/windows"
)

// StartDrag Windows - 使用 CGO 服务直接触发拖拽
func StartDrag(filePath string) error {
	fmt.Printf("🚀 CGO SERVICE: Triggering drag for: %s\n", filePath)

	// 将 Go 字符串转换为 C 字符串
	cFilePath := C.CString(filePath)
	defer C.free(unsafe.Pointer(cFilePath))

	// 调用 C 拖拽服务
	result := C.triggerDrag(cFilePath)
	if result == 0 {
		return fmt.Errorf("CGO drag service failed to initiate drag")
	}

	fmt.Println("✅ CGO SERVICE: Drag initiated successfully.")
	return nil
}

func IsAvailable() bool {
	return C.isDragAvailable() == 1
}

// 初始化拖拽服务 - 在程序启动时调用
func InitializeService() error {
	result := C.startDragService()
	if result == 0 {
		return fmt.Errorf("Failed to start drag service")
	}
	return nil
}

// 清理拖拽服务 - 在程序关闭时调用
func CleanupService() {
	C.stopDragService()
}

// 设置主线程信息 - 将当前Go线程信息传递给C++
func SetMainThreadInfo() error {
	// 获取当前线程ID
	threadID := windows.GetCurrentThreadId()

	// 获取当前进程句柄 - windows.CurrentProcess() 返回当前进程的伪句柄
	procHandle := windows.CurrentProcess()

	// 调用C++函数设置主线程信息
	C.setMainThreadInfo(C.DWORD(threadID), C.HANDLE(unsafe.Pointer(uintptr(procHandle))))

	fmt.Printf("✅ Main thread info set - ThreadID: %d\n", threadID)
	return nil
}
