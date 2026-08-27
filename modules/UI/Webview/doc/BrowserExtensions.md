# WebView2 浏览器扩展加载

`Shin::UI::WebviewWrapper` 支持在 Windows/WebView2 中安装本地**解包扩展**（unpacked browser extension）。该能力仅由宿主配置和 C++ API 控制，网页 JavaScript 不能请求任意本地目录的扩展安装。

## 配置

在 `manifest.toml` 的 `[Webview]` 节中设置：

```toml
[Webview]
BrowserExtensionsEnabled = "true"
BrowserExtensionPaths = [
  "extensions/my-extension"
]
```

- `BrowserExtensionsEnabled` 默认值为 `false`。只有显式设为字符串 `"true"` 才会启用。
- `BrowserExtensionPaths` 是字符串数组。
- 相对路径相对于程序 exe 所在目录解析；绝对路径也可使用。
- 每个目录必须是解包扩展的顶层目录，且直接包含 `manifest.json`。
- 部署时必须把扩展完整目录复制到应用程序目录或配置的绝对路径。

典型扩展目录：

```text
extensions/my-extension/
  manifest.json
  content.js
  background.js
```

## 启动流程

1. `WebviewModule::OnConfigLoad()` 读取开关和目录列表，并设置 WebView2 Profile 缓存目录。
2. `WebviewModule::OnInitialize()` 在创建 WebView 前调用：

   ```cpp
   webview.SetBrowserExtensionsEnabled(true);
   ```

3. WebView2 创建 Environment 时设置：

   ```cpp
   ICoreWebView2EnvironmentOptions6::put_AreBrowserExtensionsEnabled(TRUE);
   ```

   该操作必须早于 `webview::webview` 的构造完成；扩展默认处于禁用状态。
4. 原生 WebView 初始化后，模块对每个配置路径调用：

   ```cpp
   webview.AddBrowserExtension(extensionPath, callback);
   ```

5. `AddBrowserExtension` 通过 `ICoreWebView2_13` 获取 Profile，并查询 `ICoreWebView2Profile7`，最后调用 `AddBrowserExtension()`。
6. 所有异步安装回调结束后，模块才导航到 `Webview.startup_url`，以使匹配该页面的内容脚本在首次导航时即可执行。

扩展安装状态会持久化到 `WEBVIEW2_USER_DATA_FOLDER` 指向的 WebView2 Profile。对相同目录重新调用安装会重新安装该扩展；若修改已经安装目录的文件，WebView2 会移除该扩展，重启应用后会根据配置重新安装。

## C++ API

```cpp
using Result = Shin::UI::WebviewWrapper::BrowserExtensionResult;

webview.SetBrowserExtensionsEnabled(true); // 必须在 Initialize 前
webview.Initialize();
webview.AddBrowserExtension("C:/path/to/extension", [](const Result& result) {
    if (result.success) {
        // result.name: 扩展名称
    } else {
        // result.path / result.hresult / result.error: 错误信息
    }
});
```

`AddBrowserExtension` 会自动切换到 WebView UI/STA 线程。调用被接受后结果在异步回调中返回；方法立刻返回 `false` 则代表目录、初始化状态或扩展 API 前置条件不满足。

## 限制和排查

- 该能力仅支持 Windows 的 WebView2 后端。
- 需要含有 `ICoreWebView2Profile7` 的 WebView2 Evergreen Runtime；项目使用的 SDK 为 `1.0.2592.51`。
- WebView2 没有桌面 Edge 的工具栏，扩展可安装，但工具栏图标、popup 和 badge 没有宿主 UI 入口。建议通过 `content_scripts` 注入页面标记或页面内 UI 来验证运行。
- 若日志显示扩展 API 不可用，更新 WebView2 Evergreen Runtime。
- 若 Environment 创建返回 `ERROR_INVALID_STATE`，关闭使用同一 `WEBVIEW2_USER_DATA_FOLDER` 的旧进程后再运行；同一 Profile 的并行 Environment 必须使用一致的扩展开关。
- 若安装失败，确认 `manifest.json` 位于所配目录顶层，且应用用户对目录有读取权限。日志会输出扩展路径和 HRESULT。
