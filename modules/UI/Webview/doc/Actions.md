# Shin Webview 业务动作 (Actions) 注册表

本文档用于统一记录前端 JS 与 C++ 后端之间通信的所有业务类型（`action` 字段）。
前后端开发人员在新增业务接口时，请同步更新此文档。

## 1. 系统级 Action

| 动作名称 (`action`) | 发送方 | 描述说明 | 附带参数 / 响应结构 |
| :--- | :---: | :--- | :--- |
| **`ErrorReport`** | C++ | **统一错误上报**。当 C++ 发生系统级错误（如前端发来的 JSON 不合法、未知的业务类型等）时，会主动向前端推送此 Action。 | 包含 `msg` (具体的错误描述字符串)。如果前端发送了 `msgIndex` 则会原样透传返回。 |

## 2. 业务级 Action 概览

*(新增业务请在此表格中追加)*

| 动作 (`action`) | 发起方 | 描述说明 | 请求示例 | 响应示例 | 备注 |
| :--- | :---: | :--- | :--- | :--- | :--- |
| **`CreateSharedMemory`** | JS | 请求 C++ 创建共享内存 | `{ "action": "CreateSharedMemory", "msgIndex": "req-123", "size": 1024 }` | `{ "action": "CreateSharedMemory", "msgIndex": "req-123", "id": 1, "size": 1024 }` | 不走普通 message。JS 必须监听 `sharedbufferreceived` 事件。通过 `event.getBuffer()` 获取共享内存底层的 `ArrayBuffer` 指针，而这个响应示例 JSON 存储在 `event.additionalData` 属性中。 |
| **`DestroySharedMemory`** | JS | 请求 C++ 销毁指定共享内存 | `{ "action": "DestroySharedMemory", "id": 1 }` | `{ "action": "DestroySharedMemory", "id": 1 }` | 配合 `window.chrome.webview.releaseBuffer` 一起使用才能让系统彻底释放内存。 |
| **`GetSharedMemory`** | JS | 请求 C++ 获取已存在的共享内存 | `{ "action": "GetSharedMemory", "msgIndex": "req-124", "id": 1 }` | `{ "action": "GetSharedMemory", "msgIndex": "req-124", "id": 1, "size": 1024 }` | 不走普通 message。JS 必须监听 `sharedbufferreceived` 事件。和创建类似，通过事件对象获取 `ArrayBuffer` 指针和 JSON 附加数据。 |
| **`WindowMinimize`** | JS | 请求最小化宿主窗口 | `{ "action": "WindowMinimize" }` | 无响应 | 直接调用系统API最小化窗口 |
| **`WindowToggleMaximize`** | JS | 请求切换窗口的最大化/还原状态 | `{ "action": "WindowToggleMaximize" }` | 无响应 | 如果已最大化则还原，否则最大化 |
| **`WindowClose`** | JS | 请求关闭宿主窗口 | `{ "action": "WindowClose" }` | 无响应 | 发送 WM_CLOSE 消息关闭窗口 |
| **`WindowOpenDevTools`** | JS | 请求唤出开发者工具控制台 | `{ "action": "WindowOpenDevTools" }` | 无响应 | WebView2 仅支持唤出控制台，无法主动关闭 |
| **`WindowSetSize`** | JS | 动态设置窗口尺寸和缩放限制 | `{ "action": "WindowSetSize", "width": 800, "height": 600, "fixed": false }` | 无响应 | 支持设置窗口宽高及是否固定尺寸 |
| **`SharedMemoryUpdate`** | 双向 | 单向主动通知。告知对方某块内存已更新（或扩容） | `{ "action": "SharedMemoryUpdate", "id": 1 }` (JS 发送不带 size) | `{ "action": "SharedMemoryUpdate", "id": 1, "size": 2048 }` (C++ 推送) | 这是单向通知，不要求回应（Ack）。C++ 发送时会附带新的内存句柄，前端需读取并更新；JS 发送时仅表示内容已更新。 |

---

## 3. 业务接口详细定义

以下为各个具体业务接口的入参和出参字段详细说明。

### 3.1 CreateSharedMemory

向 C++ 申请一块指定大小的共享内存。创建成功后，底层会将该内存块的句柄直接发往前端，前端可以借此实现**零拷贝 (Zero-Copy)** 的数据读写。

#### 请求字段 (Request)

| 字段名 | 数据类型 | 必填 | 默认值 | 说明 |
| :--- | :--- | :---: | :--- | :--- |
| `action` | String | **是** | - | 动作标识，固定为 `"CreateSharedMemory"` |
| `msgIndex` | String | *否* | - | 消息索引号，用于前端异步回调关联。C++ 不做类型校验，原样透传返回。 |
| `size` | Number | **是** | - | 申请分配的共享内存大小（单位：字节） |

#### 响应字段 (Response)

> **⚠️ 特别注意：**
> 该响应不会通过常规的 `window.chrome.webview.addEventListener('message')` 收到。
> 前端必须监听专属的共享内存事件。在事件回调中，你可以同时拿到 **JSON 数据** 以及 **共享内存对象 (ArrayBuffer)**：
> 
> ```javascript
> window.chrome.webview.addEventListener('sharedbufferreceived', (event) => {
>     // 1. 获取由 C++ 回传的 JSON 响应数据 (兼容对象和字符串格式)
>     const data = typeof event.additionalData === 'string' ? JSON.parse(event.additionalData) : event.additionalData;
>     
>     if (data.action === "CreateSharedMemory") {
>         // 通过 data.msgIndex 匹配前端发出的 Promise
>         console.log("分配成功! req:", data.msgIndex, " ID:", data.id, "大小:", data.size);
>         
>         // 2. 获取真实的共享内存对象 (ArrayBuffer 类型)
>         const sharedBuffer = event.getBuffer();
>         
>         // 3. 将其封装为强类型数组以便读写数据 (零拷贝!)
>         // 例如当作无符号字节数组使用：
>         const view = new Uint8Array(sharedBuffer);
>         
>         // 现在你可以直接通过 view[0] = 255; 等操作与 C++ 共享这块内存了
>     }
> });
> ```

| 字段名 | 数据类型 | 必填 | 说明 |
| :--- | :--- | :---: | :--- |
| `action` | String | **是** | 原样返回请求的 action 名称 `"CreateSharedMemory"` |
| `msgIndex` | String | *否* | 如果请求中携带了该字段，则原样返回 |
| `id` | Number | **是** | C++ 底层自动分配并维护的共享内存块唯一标识 ID |
| `size` | Number | **是** | 实际成功分配的内存字节大小 |

---

### 3.2 DestroySharedMemory

向 C++ 请求销毁之前分配好的共享内存块。

#### 请求字段 (Request)

| 字段名 | 数据类型 | 必填 | 默认值 | 说明 |
| :--- | :--- | :---: | :--- | :--- |
| `action` | String | **是** | - | 动作标识，固定为 `"DestroySharedMemory"` |
| `msgIndex` | String | *否* | - | 消息索引号，用于前端异步回调关联。C++ 原样透传返回。 |
| `id` | Number | **是** | - | 要销毁的共享内存 ID |

#### 响应字段 (Response)

> **⚠️ 特别注意：彻底释放内存的双边责任**
> 底层共享内存对象 (`ICoreWebView2SharedBuffer`) 是基于 COM 引用计数的。
> C++ 端销毁，仅仅是将 C++ 侧的引用解绑；
> 如果 JS 侧一直保留着对 `ArrayBuffer` 及其视图的引用，或者未通知浏览器引擎释放它，这块物理内存**依然不会被系统回收**（造成内存泄漏）。
> 
> **正确的销毁姿势：**
> ```javascript
> // 1. 发送销毁指令给 C++ 端
> window.Shin.sendDataToCpp({ action: "DestroySharedMemory", id: 1 });
> 
> // 2. 主动切断 JS 端的视图引用，避免垃圾回收拦截
> myUint8ArrayView = null;
> 
> // 3. 强制通知 WebView2 引擎释放底层 COM 对象！！！
> window.chrome.webview.releaseBuffer(mySharedBuffer);
> mySharedBuffer = null;
> ```

| 字段名 | 数据类型 | 必填 | 说明 |
| :--- | :--- | :---: | :--- |
| `action` | String | **是** | 原样返回请求的 action 名称 `"DestroySharedMemory"` |
| `msgIndex` | String | *否* | 如果请求中携带了该字段，则原样返回 |
| `id` | Number | **是** | 成功销毁的共享内存块 ID |

---

### 3.3 GetSharedMemory

向 C++ 请求获取已经分配好的共享内存块句柄。这在跨页面/跨生命周期恢复连接时非常有用。

#### 请求字段 (Request)

| 字段名 | 数据类型 | 必填 | 默认值 | 说明 |
| :--- | :--- | :---: | :--- | :--- |
| `action` | String | **是** | - | 动作标识，固定为 `"GetSharedMemory"` |
| `msgIndex` | String | *否* | - | 消息索引号，用于前端异步回调关联。C++ 原样透传返回。 |
| `id` | Number | **是** | - | 要获取的共享内存 ID |

#### 响应字段 (Response)

> **⚠️ 特别注意：**
> 和创建时一样，该响应会触发专属的共享内存事件：
> `window.chrome.webview.addEventListener('sharedbufferreceived', (event) => { ... })`
> 你可以在此回调中拿到 `event.additionalData`（包含下面的 JSON 数据）和 `event.getBuffer()`（包含底层的 `ArrayBuffer`）。

| 字段名 | 数据类型 | 必填 | 说明 |
| :--- | :--- | :---: | :--- |
| `action` | String | **是** | 原样返回请求的 action 名称 `"GetSharedMemory"` |
| `msgIndex` | String | *否* | 如果请求中携带了该字段，则原样返回 |
| `id` | Number | **是** | 共享内存块的 ID |
| `size` | Number | **是** | 该内存块的字节大小 |

---

### 3.4 SharedMemoryUpdate (双向单向推送)

这是一个无需对方回复（无 Ack）的单向通知机制，用于双端共享内存的数据同步。

#### 1. C++ 向 JS 推送 (C++ -> JS)

当 C++ 对现有内存进行了**内容更新**、**动态扩容 (Resize)** 或重新分配时，派发此 Action。如果是扩容，原有的物理内存地址会发生改变，C++ 会将最新的内存句柄推给前端。

> **⚠️ 特别注意：**
> 如果是重分配/扩容，前端收到此推送后，**必须**立刻释放对旧内存的引用，并接管新的内存句柄，否则会导致浏览器泄漏。
> 收到更新后，前端可以直接从 ArrayBuffer 中读取最新写入的字符串或二进制数据。

| 字段名 | 数据类型 | 说明 |
| :--- | :--- | :--- |
| `action` | String | 固定为 `"SharedMemoryUpdate"` |
| `id` | Number | 被更新/扩容的共享内存块 ID |
| `size` | Number | 该内存块的字节大小 |

#### 2. JS 向 C++ 推送 (JS -> C++)

当前端通过 `ArrayBuffer` 直接修改了共享内存里的数据后，可以单向发一个通知告诉 C++ “我已经写完了”。

**请求示例:**
```javascript
window.Shin.sendDataToCpp({ action: "SharedMemoryUpdate", id: 1 });
```

| 字段名 | 数据类型 | 说明 |
| :--- | :--- | :--- |
| `action` | String | 固定为 `"SharedMemoryUpdate"` |
| `id` | Number | 被修改的共享内存块 ID |

### 3.5 窗口管理 (Window Management)

前端控制宿主窗口状态的相关接口。这些接口均为单向请求，执行后系统没有默认的回调响应。

#### 3.5.1 WindowMinimize
最小化当前宿主窗口。

**请求字段:**
| 字段名 | 数据类型 | 必填 | 说明 |
| :--- | :--- | :---: | :--- |
| `action` | String | **是** | 固定为 `"WindowMinimize"` |

#### 3.5.2 WindowToggleMaximize
切换当前宿主窗口的最大化/还原状态。如果当前为常规窗口，则最大化；如果已经最大化，则还原。

**请求字段:**
| 字段名 | 数据类型 | 必填 | 说明 |
| :--- | :--- | :---: | :--- |
| `action` | String | **是** | 固定为 `"WindowToggleMaximize"` |

#### 3.5.3 WindowClose
关闭当前宿主窗口。这会向主窗口发送关闭消息并结束进程。

**请求字段:**
| 字段名 | 数据类型 | 必填 | 说明 |
| :--- | :--- | :---: | :--- |
| `action` | String | **是** | 固定为 `"WindowClose"` |

#### 3.5.4 WindowOpenDevTools
唤出当前 WebView 的开发者工具控制台（DevTools）。

> **⚠️ 注意：**
> WebView2 官方 C++ API 仅提供了 `OpenDevToolsWindow` 方法，因此只能唤出和聚焦控制台，无法通过程序接口主动关闭已经打开的控制台窗口。

**请求字段:**
| 字段名 | 数据类型 | 必填 | 说明 |
| :--- | :--- | :---: | :--- |
| `action` | String | **是** | 固定为 `"WindowOpenDevTools"` |

#### 3.5.5 WindowSetSize
动态设置宿主窗口的尺寸，并可选地设置是否允许用户手动调整大小。

**请求字段:**
| 字段名 | 数据类型 | 必填 | 默认值 | 说明 |
| :--- | :--- | :---: | :--- | :--- |
| `action` | String | **是** | - | 动作标识，固定为 `"WindowSetSize"` |
| `width` | Number | **是** | - | 窗口的目标宽度（像素） |
| `height` | Number | **是** | - | 窗口的目标高度（像素） |
| `fixed` | Boolean | *否* | `false` | 是否固定窗口尺寸。为 `true` 时用户无法拖拽边缘缩放窗口。 |
