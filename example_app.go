package main

import (
	"context"
	"fmt"
	"log"
	"path/filepath"

	"github.com/562589540/WailsDragOut/pkg/drag"

	"github.com/wailsapp/wails/v2/pkg/runtime"
)

// App struct
type App struct {
	ctx context.Context
}

// NewApp creates a new App application struct
func NewApp() *App {
	return &App{}
}

// startup is called when the app starts. The context is saved
// so we can call the runtime methods
func (a *App) startup(ctx context.Context) {
	a.ctx = ctx

	// 🎯 关键：程序启动时立即初始化拖拽服务线程
	log.Println("🚀 Initializing drag service on startup...")
	err := drag.InitializeService()
	if err != nil {
		log.Printf("❌ Failed to initialize drag service: %v", err)
	} else {
		log.Println("✅ Drag service initialized successfully - ready for drag operations!")
	}

	// 🚀 NEW: 设置主线程信息，不依赖窗口标题
	log.Println("🚀 Setting main thread info...")
	err = drag.SetMainThreadInfo()
	if err != nil {
		log.Printf("❌ Failed to set main thread info: %v", err)
	} else {
		log.Println("✅ Main thread info set successfully!")
	}

	// 监听Web拖拽事件 - 在一个新的 goroutine 中运行以避免阻塞UI
	runtime.EventsOn(a.ctx, "start-drag", func(optionalData ...interface{}) {
		if len(optionalData) > 0 {
			if filePath, ok := optionalData[0].(string); ok {
				log.Printf("🎯 Web drag triggered: %s", filePath)

				go func() {
					err := drag.StartDrag(filePath)
					if err != nil {
						log.Printf("❌ Drag failed: %v", err)
					} else {
						log.Printf("✅ Drag completed successfully!")
					}
				}()
			}
		}
	})
}

// shutdown is called when the app is closing
func (a *App) shutdown(ctx context.Context) {
	// 清理拖拽服务
	drag.CleanupService()
}

// StartDrag starts file drag operation
func (a *App) StartDrag(filePath string) error {
	// Get absolute path
	absPath, err := filepath.Abs(filePath)
	if err != nil {
		return fmt.Errorf("Failed to get absolute path: %v", err)
	}

	log.Printf("Starting file drag: %s", absPath)

	// Call drag function
	return drag.StartDrag(absPath)
}
