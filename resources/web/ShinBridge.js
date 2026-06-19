/**
 * ShinBridge.js
 * 核心通信桥梁：负责 C++ 与 JS 之间的 JSON 消息传递。
 * 不包含 UI 或 渲染逻辑。
 */
class ShinBridge {
    constructor() {
        this._handlers = new Map();
        this._initEvents();
    }

    /**
     * 向 C++ 发送数据
     * @param {string} action 动作名称
     * @param {object} payload 数据负载
     * @returns {Promise<string>} 返回 C++ 处理后的响应
     */
    async send(action, payload = {}) {
        // 优先查找 window.Shin.sendDataToCpp (WebviewWrapper 映射后的路径)
        const sendFunc = (window.Shin && window.Shin.sendDataToCpp) || window.sendDataToCpp || window.__sendDataToCpp__;
        
        if (typeof sendFunc === 'function') {
            try {
                // 根据 C++ BindFunction/bind 的特性，参数会被自动序列化
                const response = await sendFunc({ action, ...payload });
                return response;
            } catch (err) {
                console.error("ShinBridge: 发送数据到 C++ 失败:", err);
                throw err;
            }
        } else {
            console.warn("ShinBridge: C++ 桥接函数未定义 (window.Shin.sendDataToCpp)。");
            return null;
        }
    }

    /**
     * 注册 C++ 主动推送消息的处理器 (C++ -> JS)
     * @param {string} action 动作名称 (JSON 中的 action 字段)
     * @param {Function} callback 处理函数
     */
    on(action, callback) {
        if (!this._handlers.has(action)) {
            this._handlers.set(action, []);
        }
        this._handlers.get(action).push(callback);
    }

    /**
     * 内部初始化事件监听
     * @private
     */
    _initEvents() {
        if (window.chrome && window.chrome.webview) {
            // 1. 监听 C++ SendJson 发送的常规消息
            window.chrome.webview.addEventListener('message', (event) => {
                const data = event.data;
                const action = data.action; 

                if (action && this._handlers.has(action)) {
                    this._handlers.get(action).forEach(cb => cb(data));
                }
            });

            // 2. 监听 SharedBuffer 原始数据（高性能视频流）
            window.chrome.webview.addEventListener('sharedbufferreceived', (e) => {
                const detail = {
                    buffer: e.getBuffer(),
                    meta: e.additionalData
                };

                // 这里我们仍然通过 shin.on 来分发 sharedbuffer 事件，保持一致性
                if (this._handlers.has('sharedbuffer')) {
                    this._handlers.get('sharedbuffer').forEach(cb => cb(detail));
                }
            });
        }
    }
}

// 创建全局单例
window.shin = new ShinBridge();
