# Shin Webview 业务动作 (Actions) 注册表

本文档用于统一记录前端 JS 与 C++ 后端之间通信的所有业务类型（`action` 字段）。
前后端开发人员在新增业务接口时，请同步更新此文档。

## 1. 通信协议规范

所有从前端发送到 C++ 的请求必须遵循以下 JSON 结构：

| 字段名 | 类型 | 必填 | 说明 |
| :--- | :--- | :---: | :--- |
| `action` | String | **是** | 动作标识，决定 C++ 执行哪个业务逻辑 |
| `msgIndex` | String | **是** | 消息唯一索引。由前端生成，C++ 在返回响应时**必须原样透传回前端**，用于匹配 Promise。 |

C++ 的响应/推送结构：
*   **响应 (Response)**：必须携带请求时的 `msgIndex`。
*   **主动推送 (Notification)**：`msgIndex` 可为空或为固定特殊值。

## 2. 系统级 Action

| 动作名称 (`action`) | 发送方 | 描述说明 | 附带参数 / 响应结构 |
| :--- | :---: | :--- | :--- |
| **`ErrorReport`** | C++ | **统一错误上报** | 包含 `msgIndex` (透传) 和 `msg` (错误描述)。 |

## 3. 业务级 Action

| 动作 (`action`) | 发起方 | 描述说明 | 请求示例 | 响应示例 | 备注 |
| :--- | :---: | :--- | :--- | :--- | :--- |
| **`CreateSharedMemory`** | JS | 请求 C++ 创建共享内存 | `{ "action": "CreateSharedMemory", "msgIndex": "uuid-123", "size": 1024 }` | `{ "action": "CreateSharedMemory", "msgIndex": "uuid-123", "id": 1, "size": 1024 }` | 响应通过 `sharedbufferreceived` 接收。 |
| **`DestroySharedMemory`** | JS | 请求 C++ 销毁共享内存 | `{ "action": "DestroySharedMemory", "msgIndex": "uuid-124", "id": 1 }` | `{ "action": "DestroySharedMemory", "msgIndex": "uuid-124", "id": 1 }` | - |
| **`WindowMinimize`** | JS | 最小化窗口 | `{ "action": "WindowMinimize", "msgIndex": "uuid-125" }` | 无响应 | 直接操作 |
| **`WindowToggleMaximize`** | JS | 最大化/还原切换 | `{ "action": "WindowToggleMaximize", "msgIndex": "uuid-126" }` | 无响应 | - |
| **`HikvisionStreamConnect`** | JS | 连接海康摄像头 | `{ "action": "HikvisionStreamConnect", "msgIndex": "uuid-127", "ip": "...", ... }` | `{ "action": "HikvisionStreamConnect", "msgIndex": "uuid-127", "id": 1, "width": 1920, ... }` | 响应携带首帧 Buffer 句柄。 |
| **`HikvisionStreamDisconnect`** | JS | 断开海康连接 | `{ "action": "HikvisionStreamDisconnect", "msgIndex": "uuid-128", "id": 1 }` | `{ "action": "HikvisionStreamDisconnect", "msgIndex": "uuid-128", "id": 1 }` | - |
| **`SharedMemoryUpdate`** | C++ | 图像数据更新通知 | - | `{ "action": "SharedMemoryUpdate", "msgIndex": "", "id": 1, "width": 1920, ... }` | 持续推送，不带 `msgIndex`。 |

### 3.1 CreateSharedMemory

向 C++ 申请一块指定大小的共享内存。创建成功后，底层会将该内存块的句柄直接发往前端，前端可以借此实现**零拷贝 (Zero-Copy)** 的数据读写。

#### 申请大小建议
为了防止读写冲突，建议申请的大小为 `数据大小 + 4 字节`。这额外的 4 字节位于 Buffer 的最前端（偏移量 0-3），用于存放**原子状态标志位**。

#### 请求字段 (Request)
| 字段名 | 数据类型 | 必填 | 说明 |
| :--- | :--- | :---: | :--- |
| `action` | String | **是** | 动作标识，固定为 `"CreateSharedMemory"` |
| `msgIndex` | String | **是** | 消息索引号，用于前端异步回调关联。C++ 必须原样透传返回。 |
| `size` | Number | **是** | 申请分配的共享内存总大小（建议包含 4 字节同步头） |

#### 响应字段 (Response)
> **⚠️ 特别注意：**
> 1. 该响应不会通过常规的 `message` 事件收到。必须监听 `sharedbufferreceived` 事件。
> 2. **自动解析**：`event.additionalData` 在抵达 JS 时已由 WebView2 自动解析为 JavaScript 对象，**严禁再次调用 `JSON.parse()`**，否则会导致对象转字符串报错。

| 字段名 | 数据类型 | 必填 | 说明 |
| :--- | :--- | :---: | :--- |
| `action` | String | **是** | 原样返回请求的 action 名称 `"CreateSharedMemory"` |
| `msgIndex` | String | **是** | 原样返回请求时的索引号 |
| `id` | Number | **是** | C++ 底层自动分配并维护的共享内存块唯一标识 ID |
| `size` | Number | **是** | 实际成功分配的内存字节大小 |

#### 同步机制 (Atomic Synchronization)
由于 C++ 和 JS 运行在不同进程且并发操作同一块内存，必须通过 Buffer 前 4 字节的原子标志位来防止读写冲突。

**状态定义：**
*   `0 (IDLE)`: 缓冲区空闲。C++ 可写入，JS 此时不应读取。
*   `1 (WRITING)`: C++ 锁定中。正在写入数据。
*   `2 (READY)`: 数据就绪。C++ 写入完成，等待 JS 读取。
*   `3 (READING)`: JS 锁定中。正在从 Buffer 读取或渲染。

**JS 端操作示例：**
```javascript
chrome.webview.addEventListener('sharedbufferreceived', (e) => {
    const buffer = e.getBuffer();
    const meta = e.additionalData; // 自动解析的对象
    
    const stateArray = new Int32Array(buffer, 0, 1);
    const dataArray = new Uint8Array(buffer, 4);

    function render() {
        if (Atomics.load(stateArray, 0) === 2) {
            if (Atomics.compareExchange(stateArray, 0, 2, 3) === 2) {
                // 绘制逻辑...
                Atomics.store(stateArray, 0);
            }
        }
    }
});
```
