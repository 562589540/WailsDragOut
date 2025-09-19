//go:build darwin
// +build darwin

package drag

/*
#cgo CFLAGS: -x objective-c
#cgo LDFLAGS: -framework Cocoa -framework Foundation

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>


// 接管用户拖拽，直接设置剪贴板
int startDragFromWindow(const char* filePath) {
    @autoreleasepool {
        NSString *path = [NSString stringWithUTF8String:filePath];
        NSURL *fileURL = [NSURL fileURLWithPath:path];

        // 检查文件是否存在
        if (![[NSFileManager defaultManager] fileExistsAtPath:path]) {
            NSLog(@"文件不存在: %@", path);
            return 0;
        }

        // 立即设置拖拽剪贴板数据
        NSPasteboard *dragBoard = [NSPasteboard pasteboardWithName:NSDragPboard];
        [dragBoard clearContents];
        [dragBoard writeObjects:@[fileURL]];

        NSLog(@"已接管拖拽，设置文件数据: %@", path);
        return 1;
    }
}


// 检查拖拽系统是否可用
int isDragAvailable() {
    NSApplication *app = [NSApplication sharedApplication];
    return (app != nil && [app isRunning]) ? 1 : 0;
}

*/
import "C"
import (
	"fmt"
	"os"
	"unsafe"
)

// StartDrag 开始拖拽文件到其他应用程序
func StartDrag(filePath string) error {
	// 检查文件是否存在
	if _, err := os.Stat(filePath); os.IsNotExist(err) {
		return fmt.Errorf("文件不存在: %s", filePath)
	}

	// 检查拖拽系统是否可用
	if C.isDragAvailable() == 0 {
		return fmt.Errorf("拖拽系统不可用")
	}

	// 转换路径为C字符串
	cPath := C.CString(filePath)
	defer C.free(unsafe.Pointer(cPath))

	// 执行拖拽（使用剪贴板和Finder方法）
	result := C.startDragFromWindow(cPath)
	if result == 0 {
		return fmt.Errorf("拖拽操作失败")
	}

	return nil
}

// IsAvailable 检查拖拽功能是否可用
func IsAvailable() bool {
	return C.isDragAvailable() == 1
}

// 初始化拖拽服务 - 在程序启动时调用
func InitializeService() error {
	return nil
}

// 清理拖拽服务 - 在程序关闭时调用
func CleanupService() {

}

// 设置主线程信息 - macOS平台不需要特殊处理
func SetMainThreadInfo() error {
	return nil
}
