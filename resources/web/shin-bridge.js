/**
 * ShinBridge.js - Unified JS-to-C++ Communication SDK
 * Implements Asynchronous Request-Response matching and Event Dispatching.
 */

class ShinBridge {
    constructor() {
        this._pendingRequests = new Map();
        this._eventHandlers = new Map();
        this._initListeners();
    }

    /**
     * Call a Native C++ Action and wait for response
     */
    async callNative(action, payload = {}) {
        const msgIndex = this._generateId();
        const request = { action, msgIndex, ...payload };

        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                if (this._pendingRequests.has(msgIndex)) {
                    this._pendingRequests.delete(msgIndex);
                    reject(new Error(`Action [${action}] timed out (msgIndex: ${msgIndex})`));
                }
            }, 10000);

            this._pendingRequests.set(msgIndex, { resolve, reject, timer });

            try {
                if (window.chrome && window.chrome.webview) {
                    window.chrome.webview.postMessage(request);
                } else {
                    throw new Error("WebView2 context not found");
                }
            } catch (err) {
                clearTimeout(timer);
                this._pendingRequests.delete(msgIndex);
                reject(err);
            }
        });
    }

    /**
     * Send a Native C++ Action without waiting for response (Fire and Forget)
     */
    postNative(action, payload = {}) {
        const request = { action, ...payload };
        if (window.chrome && window.chrome.webview) {
            window.chrome.webview.postMessage(request);
        }
    }

    on(action, callback) {
        if (!this._eventHandlers.has(action)) this._eventHandlers.set(action, []);
        this._eventHandlers.get(action).push(callback);
    }

    off(action, callback) {
        const handlers = this._eventHandlers.get(action);
        if (handlers) {
            const idx = handlers.indexOf(callback);
            if (idx !== -1) handlers.splice(idx, 1);
        }
    }

    // --- Private ---

    _initListeners() {
        const attach = () => {
            if (window.chrome && window.chrome.webview) {
                window.chrome.webview.addEventListener('message', (e) => {
                    this._handleIncomingData(e.data, null);
                });

                window.chrome.webview.addEventListener('sharedbufferreceived', (e) => {
                    this._handleIncomingData(e.additionalData, e.getBuffer());
                });
                console.log("ShinBridge: Native listeners attached.");
                return true;
            }
            return false;
        };

        if (!attach()) {
            console.warn("ShinBridge: window.chrome.webview not found. Retrying...");
            const timer = setInterval(() => {
                if (attach()) clearInterval(timer);
            }, 100);
            // Limit retries to 5 seconds
            setTimeout(() => clearInterval(timer), 5000);
        }
    }

    _handleIncomingData(data, buffer) {
        let meta = data;
        // Handle cases where WebView2 might not auto-parse JSON strings
        if (typeof data === 'string') {
            try { meta = JSON.parse(data); } catch (e) { return; }
        }
        if (!meta) return;

        const { msgIndex, action } = meta;
        const result = buffer ? { ...meta, buffer } : meta;

        if (msgIndex && this._pendingRequests.has(msgIndex)) {
            const { resolve, timer } = this._pendingRequests.get(msgIndex);
            clearTimeout(timer);
            this._pendingRequests.delete(msgIndex);
            resolve(result);
        } else if (action) {
            const handlers = this._eventHandlers.get(action);
            if (handlers) handlers.forEach(fn => fn(result));
        }
    }

    _generateId() {
        return Date.now().toString(36) + Math.random().toString(36).substr(2, 5);
    }
}

// Ensure single instance
if (!window.shin) {
    window.shin = new ShinBridge();
}
