# 使用指南

## 📦 如何集成 WailsDragOut 到您的项目

### 1. 安装依赖

```bash
go get github.com/562589540/WailsDragOut
```

### 2. Go 后端集成 (app.go)

将以下代码添加到您的 Wails App 结构体中：

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

func NewApp() *App {
    return &App{}
}

// 🔑 关键：在 startup 中初始化拖拽服务
func (a *App) startup(ctx context.Context) {
    a.ctx = ctx

    // 步骤1: 初始化拖拽服务
    log.Println("🚀 Initializing drag service...")
    err := drag.InitializeService()
    if err != nil {
        log.Printf("❌ Failed to initialize drag service: %v", err)
        return
    }
    log.Println("✅ Drag service initialized!")

    // 步骤2: 设置主线程信息 (Windows 必需)
    err = drag.SetMainThreadInfo()
    if err != nil {
        log.Printf("❌ Failed to set main thread info: %v", err)
        return
    }
    log.Println("✅ Main thread info set!")

    // 步骤3: 监听前端拖拽事件
    runtime.EventsOn(a.ctx, "start-drag", func(optionalData ...interface{}) {
        if len(optionalData) > 0 {
            if filePath, ok := optionalData[0].(string); ok {
                log.Printf("🎯 Starting drag for: %s", filePath)
                
                // 在单独的 goroutine 中执行拖拽，避免阻塞UI
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

// 🔑 关键：在 shutdown 中清理服务
func (a *App) shutdown(ctx context.Context) {
    log.Println("🧹 Cleaning up drag service...")
    drag.CleanupService()
}

// 可选：提供给前端直接调用的方法
func (a *App) StartFileDrag(filePath string) error {
    return drag.StartDrag(filePath)
}
```

### 3. Vue 前端集成

在您的 Vue 组件中添加以下代码：

```vue
<script lang="ts" setup>
import { ref } from "vue";
import { EventsEmit } from "../wailsjs/runtime/runtime";

const dragStatus = ref("准备拖拽");

// 🔑 关键：处理拖拽开始事件
function handleDragStart(event: DragEvent) {
  // 步骤1: 阻止浏览器默认拖拽行为
  event.preventDefault();
  
  dragStatus.value = "🚀 正在启动系统级拖拽...";
  
  // 步骤2: 准备要拖拽的文件路径
  const filePath = getFilePathToTrag(); // 您的文件路径获取逻辑
  
  // 步骤3: 触发系统级拖拽
  console.log("通知后端开始拖拽:", filePath);
  EventsEmit("start-drag", filePath);
  
  // 步骤4: 设置最小的拖拽数据（备用）
  if (event.dataTransfer) {
    event.dataTransfer.setData("text/plain", "WailsDragOut");
    event.dataTransfer.effectAllowed = "copy";
  }
}

function handleDragEnd(event: DragEvent) {
  dragStatus.value = "拖拽完成";
  console.log("拖拽结束");
}

// 您的文件路径获取逻辑
function getFilePathToTrag(): string {
  // 示例：根据平台返回不同的测试文件
  const isWindows = navigator.platform.includes('Win');
  if (isWindows) {
    return "C:\\Users\\YourName\\Documents\\example.txt";
  } else {
    return "/Users/YourName/Documents/example.txt";
  }
}
</script>

<template>
  <div class="drag-container">
    <!-- 🔑 关键：拖拽区域 -->
    <div
      class="drag-zone"
      draggable="true"
      @dragstart="handleDragStart"
      @dragend="handleDragEnd"
    >
      <div class="drag-icon">📁</div>
      <div class="drag-title">拖拽文件</div>
      <div class="drag-subtitle">拖动到其他应用程序</div>
    </div>
    
    <!-- 状态显示 -->
    <div class="status">{{ dragStatus }}</div>
  </div>
</template>

<style scoped>
.drag-container {
  padding: 20px;
  text-align: center;
}

.drag-zone {
  background: linear-gradient(135deg, #667eea, #764ba2);
  color: white;
  border-radius: 12px;
  padding: 40px 20px;
  cursor: grab;
  user-select: none;
  transition: all 0.3s ease;
  display: inline-block;
  min-width: 200px;
}

.drag-zone:hover {
  transform: translateY(-2px);
  box-shadow: 0 8px 25px rgba(102, 126, 234, 0.3);
}

.drag-zone:active {
  cursor: grabbing;
  transform: translateY(0);
}

.drag-icon {
  font-size: 2.5em;
  margin-bottom: 10px;
}

.drag-title {
  font-size: 1.2em;
  font-weight: 600;
  margin-bottom: 5px;
}

.drag-subtitle {
  font-size: 0.9em;
  opacity: 0.9;
}

.status {
  margin-top: 20px;
  padding: 10px;
  background: #f8f9fa;
  border-radius: 8px;
  color: #495057;
}
</style>
```

## 🔧 高级配置

### 动态文件路径

```javascript
// 示例：从用户输入获取文件路径
function getSelectedFilePath() {
  const fileInput = document.querySelector('#file-input');
  if (fileInput?.files?.[0]) {
    return fileInput.files[0].path; // Electron/Tauri 环境
  }
  return "/default/path/to/file.txt";
}

function handleDragStart(event) {
  event.preventDefault();
  const selectedFile = getSelectedFilePath();
  EventsEmit("start-drag", selectedFile);
}
```

### 错误处理

```javascript
// 监听拖拽结果
import { EventsOn } from "../wailsjs/runtime/runtime";

EventsOn("drag-completed", (success, error) => {
  if (success) {
    console.log("✅ 拖拽成功完成");
  } else {
    console.error("❌ 拖拽失败:", error);
  }
});
```

### 平台检测

```javascript
function isPlatformSupported() {
  const platform = navigator.platform;
  return platform.includes('Mac') || platform.includes('Win');
}

if (!isPlatformSupported()) {
  console.warn("⚠️ 当前平台可能不支持系统级拖拽");
}
```

## 🐛 常见问题

### Q: 拖拽没有反应？
A: 检查以下几点：
1. 确保调用了 `drag.InitializeService()`
2. Windows 平台确保调用了 `drag.SetMainThreadInfo()`
3. 检查文件路径是否正确且文件存在
4. 查看控制台日志

### Q: 出现"self-drop detected"？
A: 这是正常的自拖拽检测功能，防止拖拽到自己的应用。

### Q: Windows 平台拖拽失败？
A: 确保：
1. 以管理员权限运行（某些系统文件需要）
2. 文件路径使用反斜杠 `\` 或正斜杠 `/`
3. 检查 C++ 编译环境

## 📝 调试技巧

启用详细日志：
```go
import "log"

// 在 main 函数中
log.SetFlags(log.LstdFlags | log.Lshortfile)
```

查看拖拽过程：
- macOS: 查看 Console.app 中的应用日志
- Windows: 查看控制台输出

## 🎯 最佳实践

1. **错误处理**: 始终检查 `StartDrag` 的返回值
2. **异步执行**: 在 goroutine 中执行拖拽操作
3. **路径验证**: 拖拽前验证文件是否存在
4. **用户反馈**: 提供清晰的拖拽状态提示
5. **资源清理**: 应用关闭时调用 `CleanupService()`
