<script lang="ts" setup>
import { ref } from "vue";
import { EventsEmit } from "../wailsjs/runtime/runtime";

const status = ref("拖拽我到其他应用试试！");
const isWindows = ref(false);
const isDragging = ref(false);

// 用户开始拖拽时触发
function handleDragStart(event: DragEvent) {
  // 关键：阻止浏览器的默认拖拽行为，避免资源冲突！
  event.preventDefault();

  status.value = "🚀 正在接管拖拽...";

  let testPath = "";
  if (isWindows.value) {
    testPath = "D:\\2.png";
  } else {
    testPath = "/Users/username/Downloads/example.jpg";
  }

  // 浏览器拖拽已被阻止，现在可以安全地通知后端来接管
  console.log("通知后端接管拖拽");
  EventsEmit("start-drag", testPath);

  // 保持一个最小化的数据，以防万一
  if (event.dataTransfer) {
    event.dataTransfer.setData("text/plain", "Wails Drag Handover");
    event.dataTransfer.effectAllowed = "copy";
  }
}

// 拖拽结束
function handleDragEnd(event: DragEvent) {
  console.log("拖拽结束", event);
  status.value = "拖拽完成！";
  isDragging.value = false;
}

// 系统切换
function handleSystemSwitch() {
  isWindows.value = !isWindows.value;
}
</script>

<template>
  <div class="app">
    <div class="header">
      <h1>🚀 WailsDragOut 示例</h1>
      <p>跨平台文件拖拽库</p>
    </div>

    <div class="content">
      <div class="button-group">
        <!-- 系统切换 -->
        <button @click="handleSystemSwitch">
          切换系统: {{ isWindows ? 'Windows' : 'macOS' }}
        </button>
      </div>
      
      <!-- 核心拖拽区域 -->
      <div class="drag-section">
        <div
          class="drag-zone"
          draggable="true"
          @dragstart="handleDragStart"
          @dragend="handleDragEnd"
        >
          <div class="drag-icon">🎯</div>
          <div class="drag-title">拖拽我</div>
          <div class="drag-subtitle">直接拖动到其他应用</div>
        </div>

        <!-- 简单状态显示 -->
        <div class="simple-status">{{ status }}</div>
      </div>
    </div>

    <div class="footer">
      <p v-if="!isWindows">🎯 macOS: 完美支持系统级拖拽！</p>
      <p v-else>✅ Windows: 支持系统级拖拽（带自拖拽检测）</p>
    </div>
  </div>
</template>

<style scoped>
* {
  box-sizing: border-box;
}

.app {
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
  max-width: 800px;
  margin: 0 auto;
  padding: 20px;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  min-height: 100vh;
  color: #333;
}

.header {
  text-align: center;
  margin-bottom: 30px;
  color: white;
}

.header h1 {
  font-size: 2.5em;
  margin-bottom: 10px;
}

.header p {
  font-size: 1.1em;
  opacity: 0.9;
}

.content {
  background: rgba(255, 255, 255, 0.95);
  border-radius: 15px;
  padding: 30px;
  backdrop-filter: blur(10px);
}

.button-group {
  text-align: center;
  margin-bottom: 20px;
}

.button-group button {
  background: #667eea;
  color: white;
  border: none;
  padding: 10px 20px;
  border-radius: 8px;
  cursor: pointer;
  font-size: 1em;
}

.button-group button:hover {
  background: #5a6fd8;
}

.drag-section {
  margin-bottom: 25px;
  padding: 40px;
  border-radius: 15px;
  background: rgba(255, 107, 107, 0.05);
  border: 2px solid #ff6b6b;
  text-align: center;
}

.drag-zone {
  background: linear-gradient(135deg, #ff6b6b, #ff8e8e);
  border-radius: 15px;
  padding: 30px;
  text-align: center;
  cursor: grab;
  transition: all 0.3s ease;
  color: white;
  user-select: none;
  margin: 15px 0;
}

.drag-zone:hover {
  transform: translateY(-3px) scale(1.02);
  box-shadow: 0 10px 25px rgba(255, 107, 107, 0.3);
}

.drag-zone:active {
  cursor: grabbing;
  transform: translateY(-1px) scale(1.01);
}

.drag-icon {
  font-size: 3em;
  margin-bottom: 15px;
  animation: bounce 2s infinite;
}

.drag-title {
  font-size: 1.4em;
  font-weight: 600;
  margin-bottom: 8px;
}

.drag-subtitle {
  font-size: 1em;
  opacity: 0.9;
}

@keyframes bounce {
  0%, 20%, 50%, 80%, 100% {
    transform: translateY(0);
  }
  40% {
    transform: translateY(-10px);
  }
  60% {
    transform: translateY(-5px);
  }
}

.simple-status {
  margin-top: 20px;
  padding: 15px;
  background: rgba(255, 255, 255, 0.9);
  border-radius: 10px;
  font-size: 1em;
  color: #333;
  min-height: 50px;
  display: flex;
  align-items: center;
  justify-content: center;
}

.footer {
  text-align: center;
  margin-top: 20px;
  color: rgba(255, 255, 255, 0.9);
  font-size: 0.95em;
}

@media (max-width: 600px) {
  .app {
    padding: 10px;
  }
  
  .button-group {
    flex-direction: column;
  }
}
</style>
