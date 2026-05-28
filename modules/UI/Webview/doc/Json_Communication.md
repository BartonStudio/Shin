# Webview C++ 与 JS 的 JSON 通信指南

在 Shin Webview 中，C++ 与前端 (JavaScript) 之间主要通过 JSON 格式进行双向通信。以下是双向通信的简单说明与代码示例。

---

## 1. C++ 主动发送数据给 JS (C++ -> JS)

当 C++ 后台有事件产生、状态更新或需要主动推送数据给前端时使用此方式。

### C++ 端发送 (写数据)
使用 `SendJson` 方法，直接向 Webview 传入 JSON 格式的字符串。
```cpp
#include "WebviewWrapper.hpp"

// 获取 Webview 实例
auto& webview = Shin::UI::WebviewWrapper::GetInstance();

// 构造 JSON 字符串
std::string jsonPayload = "{\"type\": \"SystemEvent\", \"message\": \"Hello from C++\"}";

// 异步发送给 JS
webview.SendJson(jsonPayload);
```

### JS 端接收 (获取数据)
在浏览器前端，监听 `window.chrome.webview` 的 `message` 事件。WebView2 引擎会自动将接收到的消息解析为 JavaScript 对象。
```javascript
if (window.chrome && window.chrome.webview) {
    window.chrome.webview.addEventListener('message', function(event) {
        // event.data 就是 C++ 传过来的 JSON 解析后的对象
        const data = event.data; 
        
        if (data.type === 'SystemEvent') {
            console.log("收到 C++ 的消息:", data.message);
        }
    });
}
```

---

## 2. JS 发送数据给 C++ (JS -> C++)

当前端页面触发某些操作（如表单提交、按钮点击），需要向后台 C++ 传递数据，并且可能需要等待结果时使用此方式。

### C++ 端接收 (绑定方法)
使用 `BindFunction` 方法，将一个全局函数名暴露给 JS。JS 调用时传入的参数会被序列化为一个 JSON 字符串（通常为数组形式）传入回调函数中。
```cpp
#include "WebviewWrapper.hpp"
#include <iostream>

auto& webview = Shin::UI::WebviewWrapper::GetInstance();

// 将函数 "sendDataToCpp" 暴露给前端 JS
webview.BindFunction("sendDataToCpp", [](const std::string& req) -> std::string {
    // req 通常是 JS 传参的 JSON 数组结构，例如: [{"id":1,"action":"start"}]
    std::cout << "获取到 JS 发来的数据: " << req << std::endl;
    
    // 业务逻辑处理...

    // 必须返回一个字符串（建议是 JSON 格式），此返回值将传递回 JS 的 Promise 中
    return "{\"status\": \"success\"}";
});
```

### JS 端发送 (写数据)
在前端，C++ 绑定的函数会挂载在 `window` 对象上，并且会被 WebView2 包装成一个返回 `Promise` 的异步函数。可以直接向其传入 JSON 对象。
```javascript
// 准备要发送的 JSON 数据
const payload = { id: 1, action: "start" };

// 检查该函数是否已经被 C++ 成功绑定
if (window.sendDataToCpp) {
    // 调用该函数，参数会被自动序列化传递给 C++
    window.sendDataToCpp(payload).then(response => {
        // response 是 C++ return 回来的字符串数据
        console.log("C++ 处理完成返回的内容:", response);
    }).catch(err => {
        console.error("调用 C++ 接口失败:", err);
    });
}
```
