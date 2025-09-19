# WailsDragOut

🚀 **跨平台系统级文件拖拽库** - 专为 Wails 应用设计

一个强大的 Go 库，让您的 Wails 应用支持真正的系统级文件拖拽功能。支持 macOS 和 Windows，具有智能自拖拽检测，防止应用内部循环拖拽。

## ✨ 特性

- 🎯 **真正的系统级拖拽** - 不依赖浏览器限制
- 🖥️ **跨平台支持** - macOS ✅ | Windows ✅  
- 🛡️ **智能自拖拽检测** - 自动防止应用内部循环拖拽
- 🔧 **CGO 优化** - 直接调用系统 API，性能优异
- 📱 **Vue 集成** - 提供完整的前端示例
- 🐛 **详细调试** - 完整的拖拽过程日志

## 🚀 快速开始

### 1. 安装

```bash
go get github.com/562589540/WailsDragOut
```

### 2. 基本用法

#### Go 后端集成

```go
package main

import (
    "context"
    "log"
    "github.com/562589540/WailsDragOut/pkg/drag"
    "github.com/wailsapp/wails/v2/pkg/runtime"
)

type App struct {
    ctx context.Context
}

func (a *App) startup(ctx context.Context) {
    a.ctx = ctx

    // 初始化拖拽服务
    log.Println("🚀 Initializing drag service...")
    err := drag.InitializeService()
    if err != nil {
        log.Printf("❌ Failed to initialize drag service: %v", err)
        return
    }

    // 设置主线程信息（Windows 必需）
    err = drag.SetMainThreadInfo()
    if err != nil {
        log.Printf("❌ Failed to set main thread info: %v", err)
        return
    }

    // 监听前端拖拽事件
    runtime.EventsOn(a.ctx, "start-drag", func(optionalData ...interface{}) {
        if len(optionalData) > 0 {
            if filePath, ok := optionalData[0].(string); ok {
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

func (a *App) shutdown(ctx context.Context) {
    // 清理拖拽服务
    drag.CleanupService()
}
```

#### Vue 前端集成

```vue
<script lang="ts" setup>
import { ref } from "vue";
import { EventsEmit } from "../wailsjs/runtime/runtime";

const status = ref("拖拽我到其他应用试试！");

function handleDragStart(event: DragEvent) {
  // 阻止浏览器默认拖拽行为
  event.preventDefault();
  
  status.value = "🚀 正在接管拖拽...";
  
  // 触发系统级拖拽
  const filePath = "/path/to/your/file.jpg";
  EventsEmit("start-drag", filePath);
}

function handleDragEnd(event: DragEvent) {
  status.value = "拖拽完成！";
}
</script>

<template>
  <div
    class="drag-zone"
    draggable="true"
    @dragstart="handleDragStart"
    @dragend="handleDragEnd"
  >
    <div class="drag-title">拖拽我</div>
    <div class="drag-subtitle">直接拖动到其他应用</div>
  </div>
  <div class="status">{{ status }}</div>
</template>

<style scoped>
.drag-zone {
  background: linear-gradient(135deg, #ff6b6b, #ff8e8e);
  border-radius: 15px;
  padding: 30px;
  text-align: center;
  cursor: grab;
  color: white;
  user-select: none;
}

.drag-zone:hover {
  transform: translateY(-3px);
  box-shadow: 0 10px 25px rgba(255, 107, 107, 0.3);
}

.drag-zone:active {
  cursor: grabbing;
}
</style>
```

## 📋 API 参考

### 核心功能

#### `drag.InitializeService() error`
初始化拖拽服务。必须在应用启动时调用。

#### `drag.SetMainThreadInfo() error`
设置主线程信息（Windows 专用）。用于正确的窗口查找和线程同步。

#### `drag.StartDrag(filePath string) error`
启动文件拖拽操作。
- `filePath`: 要拖拽的文件路径（支持相对路径和绝对路径）

#### `drag.CleanupService()`
清理拖拽服务。建议在应用关闭时调用。

#### `drag.IsAvailable() bool`
检查拖拽功能是否在当前平台可用。

### 平台特性

| 功能 | macOS | Windows |
|------|-------|---------|
| 基础拖拽 | ✅ | ✅ |
| 自拖拽检测 | ✅ | ✅ |
| 线程同步 | N/A | ✅ |
| 窗口查找 | 自动 | 智能枚举 |

## 🔧 高级配置

### Windows 特殊配置

Windows 平台使用了以下高级特性：

1. **多重窗口查找策略**：
   - 优先使用 `GetForegroundWindow()`
   - 备用线程ID枚举
   - 最终兜底窗口查找

2. **智能自拖拽检测**：
   - 检查窗口层次结构
   - 进程ID比较
   - 父窗口和所有者窗口遍历

3. **线程输入同步**：
   - 自动 `AttachThreadInput`
   - 拖拽完成后自动清理

## 🐛 调试

启用详细日志来诊断问题：

```bash
# 在 Windows 上，您会看到类似的调试输出：
SERVICE: Processing drag request for: D:\example.png
SERVICE: Found main window using GetForegroundWindow: 0000000000aa08c0
SERVICE: Window thread ID: 11552, Self thread ID: 12364
SERVICE: AttachThreadInput result: SUCCESS
SERVICE: Checking window 0000000000f90624 - PID: 6952 (Self: 9084)
SERVICE: Final drop target check - Is self app: NO
SERVICE: Drop target is external application - allowing drop
SERVICE: DoDragDrop completed. HRESULT: 0x00040100, dwEffect: 0x00000001
```

## 📝 注意事项

1. **文件路径**: 确保文件存在且可访问
2. **权限**: 某些系统文件可能需要特殊权限
3. **线程安全**: 拖拽操作在单独的 goroutine 中运行
4. **内存管理**: C++ 资源会自动清理

## 🤝 贡献

欢迎提交 Issues 和 Pull Requests！

## 📄 许可证

MIT License

## 🙏 致谢

- [Wails](https://wails.io/) - 出色的跨平台应用框架
- Windows Shell API 文档
- macOS Cocoa 框架文档

---

🌟 如果这个库对您有帮助，请给个 Star ⭐️
