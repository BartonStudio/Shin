# Shin — 工程上下文

> Windows 桌面应用（C++17 / CMake / WebView2）。原生 C++ 后端 + Web 前端，通过 JSON 消息 + 共享内存桥接。主要用途：海康(Hikvision) 摄像头视频流接入 + 本地日程/备忘管理。注释与提交信息多为中文。

## 构建与运行
- 生成：CMake（`build/` 已存在）。C++17，MSVC，Windows-only。
- 依赖：OpenCV 4（硬编码 `D:/OpenCV/opencv/build`），HCSDK（海康 SDK，`third_party/HCSDK`），WebView2。
- 第三方均为 git submodule（见 `.gitmodules`）：spdlog、webview、nlohmann/json、toml11、SQLiteCpp、IXWebSocket。**改动前先确认 submodule 已 checkout。**
- 输出统一到 `build/bin/{Debug,Release}`（所有 dll/exe 同目录，便于运行时找 dll）。POST_BUILD 会拷贝 HCSDK/OpenCV dll 及 `resources/web/*.js`。
- 主程序目标：`ShinApp`（`src/main.cpp`）。各模块/测试为独立 target。

## 架构总览
三层，均为 SHARED 库，靠 `Shin::` 命名空间与 alias target 链接：
- **`Shin::Core`**（`modules/Core`）— 引擎基础设施。
- **`Shin::System::*`**（`modules/System`）— 系统能力（Auth、Calendar）。
- **`Shin::UI::Webview`**（`modules/UI/Webview`）— WebView2 封装 + 前后端消息路由。
- **`Service`**（`src/Service`）— 应用级业务（VideoService）。

### 启动流程（`src/main.cpp` → `ShinCore.hpp`）
`Shin::Init()`：① `Log::Init()` ② `Config::GetInstance()`（加载 `manifest.toml`）③ `ModuleManager::InitializeAll()`。
模块通过 `SHIN_REGISTER_MODULE(Class)` 宏 + 静态注册器自动登记（实现 `IModule`）。`main` 里手动做依赖注入（`videoService.SetManager(&webview)`）并 `RegisterAction` 注册业务动作，最后 `webview.RunBlocking()`，退出时 `Shin::Shutdown()`（保存 Config、逆序 `OnShutdown`）。

## Core 模块（`modules/Core`）
- **Log**（`Log.hpp`）：`LOGI/LOGD/LOGW/LOGE/LOGT(tag) << msg` 流式日志（spdlog 后端）。支持回调 sink / JSON sink（Webview 把日志转发到前端 console）。导出宏 `SHIN_API`（`SHIN_CORE_EXPORTS`）。
- **Config**（`Config.hpp`, `IConfigurable.hpp`, `ConfigDefaults.hpp`）：单例，读写 `manifest.toml`。点分 key（`"Webview.window_width"`）。模块可实现 `IConfigurable`（`GetConfigPath`/`OnConfigLoad`/`OnConfigSave`）自动收发配置。硬编码默认值在 `ConfigDefaults.hpp::Defaults::Manifest`。退出时自动 `Save()`。
- **TaskLoop**（`TaskLoop.hpp`）：单例后台线程池，`PostTask(fn)`。自身也是一个自动注册模块。
- **Module**（`Module.hpp`）：`IModule` 接口 + `ModuleManager` 生命周期管理 + 注册宏。
- **ISharedMemoryManager**（`include/ISharedMemoryManager.hpp`）：打破 UI↔Service 循环依赖的接口，`WebviewWrapper` 实现它，`VideoService` 只依赖此接口。

## UI/Webview（`modules/UI/Webview`）
- **`WebviewWrapper`**（单例，实现 `ISharedMemoryManager`）：窗口/导航/JS 注入/绑定/共享内存。导出宏 `SHIN_UIWEBVIEW_API`（`SHIN_UIWEBVIEW_EXPORTS`）。`m_impl` 为 PImpl。
- **`WebviewMessageHandler`**（命名空间函数）：前后端消息中枢。
  - `RegisterAction(name, handler, runInBackground=true)` 注册动作；handler 签名 `(req json, res json, sendResponse cb)`。
  - `ProcessMessage` 解析前端 JSON（必须含 `"action"` 字段，可带 `msgIndex` 做请求配对），查表分发；后台动作丢进内置 4 线程池。
  - 内置动作：`CreateSharedMemory` / `DestroySharedMemory` / `GetSharedMemory` / `SharedMemoryUpdate` / `WindowMinimize` / `WindowDrag` / `WindowToggleMaximize` / `WindowClose` / `WindowOpenDevTools` / `Navigate` / `WindowSetSize`。`WebviewModule` 另注册 `ZoomIn`/`ZoomOut`。错误统一回 `{"action":"ErrorReport","msg":...}`。
- **`WebviewModule`**（`WebviewModule.cpp`）：IModule+IConfigurable，配置路径 `"Webview"`。设置 WebView2 缓存目录、无边框窗口、启动 URL、（可选）注入 `ShinDevMenu.js`、把 C++ 日志转发到前端。

### 前后端通信协议
- **JS → C++**：`window.shin.send(action, payload)` → `window.Shin.sendDataToCpp`（由 `__sendDataToCpp__` 绑定重命名而来）。见 `resources/web/ShinBridge.js`（全局单例 `window.shin`）。
- **C++ → JS**：`SendJson`（`chrome.webview` message 事件，按 `data.action` 分发）；`SendString`；共享内存走 `sharedbufferreceived` 事件（零拷贝，用于视频流）。
- 前端资源：`resources/web/`（`index.html`、`ShinBridge.js` 通信桥、`ShinDevMenu.js` 调试菜单、`HAMonitor.js`）。

## System 模块（`modules/System`）
- **Auth**（`LocalAuthenticator.hpp`）：Windows Hello 本地身份验证。`IsAvailable()` / `VerifyUser(AuthOptions)` → `AuthResult`。链接 `windowsapp`。导出宏 `SHIN_SYSTEM_API`（`SHIN_SYSTEM_EXPORTS`）。
- **Calendar**（`MemoManager.hpp`, 命名空间 `Shin::Data`）：SQLite 日程/备忘。单例，`Initialize(Config)`（db 路径配置键 `Calendar.db_path`）、`ExecuteQuery(sql)→json` / `Execute(sql)→int`。链接 `SQLiteCpp`。Schema：`resources/database/ShinCalendar.schema.toml`。
  - 配套技能 `.claude/skills/shincalendar/`（Python 脚本，从 TOML schema 建库、增删查日程）。
- **Network**（`NetworkMonitor.hpp`, 命名空间 `Shin::System`）：网络连接状态查询（Windows-only，按需拉取无后台线程）。静态类 `NetworkMonitor::GetStatus() → NetworkStatus`。判定优先级：WiFi 已连接 → `Wifi`；否则以太网在用 → `Ethernet`；都没有 → `None`。`NetworkStatus` 有效字段：`type` + WiFi 时的 `ssid` / `signalQuality`(0~100，Windows 原生口径)。实现用 WLAN API（`WlanEnumInterfaces`+`WlanQueryInterface(current_connection)` 取信号）+ IP Helper（`GetAdaptersAddresses` 判以太网）。链接 `wlanapi` + `iphlpapi`。仅报「主连接」，不做外网可达性检测。**前端 Action 尚未接入**（需要时加 `GetNetworkStatus`）。
  - 结构体里 `signalDbm`（近似 dBm）、`rxRateKbps`/`txRateKbps`（链路协商速率，非实时流量）已注释保留，需要时取消注释即可；`signalBars` 已彻底移除（前端自行由 quality 换算格数）。
  - ⚠️ 该模块 target 加了 `/utf-8` 编译选项：源码为 UTF-8 无 BOM，MSVC 在 936 代码页下会误读中文注释吞掉标识符（C4819）。新增含中文注释的 System 模块建议照做。

## Service（`src/Service/VideoService`）
海康摄像头视频流管理单例。`ConnectStream(ip,port,user,pwd,...)→id` / `DisconnectStream(id)`。通过 `ISharedMemoryManager*`（注入的 WebviewWrapper）把解码帧写入共享内存并推送前端。SDK 回调 `RealDataCallBack`/`DecCallBack`（`__stdcall`）。`main.cpp` 注册 `HikvisionStreamConnect`/`HikvisionStreamDisconnect` 动作对接前端。

## 约定 / 注意
- 每层各自的 DLL 导出宏，改头文件时保持 `#ifdef *_EXPORTS` 模式一致。
- 跨 DLL 边界的 `std::unique_ptr`/容器用 `#pragma warning(disable:4251)` 抑制。
- 单例满天飞（`GetInstance()`）；新增业务优先「注册 Action + 依赖注入接口」而非直接互相 include，避免循环依赖。
- 平台 Windows-only（`<windows.h>`、WebView2、HCSDK、Windows Hello）。
- 提交信息可用中文，与现有风格一致。

## 当前 git 状态提示（快照，会过期）
- SharedMemory 模块（原 `modules/System/SharedMemory`）已删除，共享内存能力现由 `WebviewWrapper` 经 `ISharedMemoryManager` 提供。
- 新增 `modules/System/Calendar/`、`modules/System/Network/` 与 `third_party/ixwebsocket`（IXWebSocket，尚未接入到上面各层，注意确认用途）。
