/**
 * Home Assistant WebSocket Monitor Class
 * 设计理念：
 * 1. 使用私有字段锁定 token，防止运行期篡改
 * 2. 接口化订阅：业务层决定监听什么，底层只负责连接与分发
 */
class HAMonitor {
    #token;
    #ws = null;
    #url = "ws://192.168.31.38:8123/api/websocket";
    #onStateChange = null;
    #isAuth = false;
    #pendingSubscriptions = []; // 用于断线重连后的自动恢复

    constructor(token, onStateChange) {
        if (!token) throw new Error("Token is required");
        this.#token = token;
        this.#onStateChange = onStateChange;
        this.#connect();
    }

    // 公开订阅接口
    subscribe(entityIds) {
        const ids = Array.isArray(entityIds) ? entityIds : [entityIds];
        this.#pendingSubscriptions.push(...ids);
        
        if (this.#isAuth) {
            this.#sendSubscribe(ids);
        }
    }

    #sendSubscribe(ids) {
        if (!this.#ws || this.#ws.readyState !== WebSocket.OPEN) return;
        
        const msg = {
            "id": Math.floor(Math.random() * 10000),
            "type": "subscribe_trigger",
            "trigger": {
                "platform": "state",
                "entity_id": ids
            }
        };
        this.#ws.send(JSON.stringify(msg));
    }

    #connect() {
        this.#ws = new WebSocket(this.#url);

        this.#ws.onopen = () => {
            console.log("✅ HA WebSocket 连接建立");
            this.#ws.send(JSON.stringify({
                "type": "auth",
                "access_token": this.#token
            }));
        };

        this.#ws.onmessage = (event) => {
            const data = JSON.parse(event.data);
            
            if (data.type === "auth_ok") {
                console.log("✅ HA 身份验证成功");
                this.#isAuth = true;
                // 自动恢复订阅
                if (this.#pendingSubscriptions.length > 0) {
                    this.#sendSubscribe(this.#pendingSubscriptions);
                }
            } else if (data.type === "event" && this.#onStateChange) {
                this.#onStateChange(data.event);
            }
        };

        this.#ws.onclose = () => {
            console.warn("❗ HA 连接断开，尝试重连...");
            this.#isAuth = false;
            setTimeout(() => this.#connect(), 5000);
        };
    }
}
