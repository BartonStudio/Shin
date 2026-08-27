/*
 * Home Assistant 浏览器 WebSocket 客户端
 *
 * 直连 Home Assistant 的 /api/websocket，不依赖 C++ 或宿主桥接。
 * 使用示例：
 *   await ha.connect({ baseUrl: "http://homeassistant.local:8123", accessToken: "token" });
 *   const states = await ha.getStates();
 *   await ha.callService("light", "turn_on", { target: { entity_id: "light.living_room" } });
 *   const unsubscribe = await ha.subscribeStateChanges(change => console.log(change));
 *   await unsubscribe();
 */
(function (global) {
  "use strict";

  const STORAGE_KEY = "shin.homeassistant.ws.config.v1";
  const STATUS = Object.freeze({
    IDLE: "idle",
    CONNECTING: "connecting",
    AUTHENTICATING: "authenticating",
    READY: "ready",
    RECONNECTING: "reconnecting",
    CLOSED: "closed",
    AUTH_FAILED: "auth_failed"
  });

  class HomeAssistantError extends Error {
    constructor(message, details) {
      super(message);
      this.name = "HomeAssistantError";
      this.code = details && details.code ? details.code : "unknown_error";
      this.raw = details && details.raw ? details.raw : null;
    }
  }

  class HomeAssistantWs {
    constructor() {
      this._socket = null;
      this._status = STATUS.IDLE;
      this._serverVersion = null;
      this._config = null;
      this._nextId = 1;
      this._pending = new Map();
      this._subscriptions = new Set();
      this._subscriptionsById = new Map();
      this._connectionListeners = new Set();
      this._connectPromise = null;
      this._connectResolve = null;
      this._connectReject = null;
      this._manualDisconnect = false;
      this._authFailed = false;
      this._reconnectAttempts = 0;
      this._reconnectTimer = null;
      this._heartbeatTimer = null;
      this._heartbeatPending = null;
    }

    get status() {
      return this._status;
    }

    get isReady() {
      return this._status === STATUS.READY;
    }

    get serverVersion() {
      return this._serverVersion;
    }

    get config() {
      return this._config ? { baseUrl: this._config.baseUrl } : null;
    }

    onConnectionState(handler) {
      if (typeof handler !== "function") {
        throw new TypeError("onConnectionState 的 handler 必须是函数。");
      }
      this._connectionListeners.add(handler);
      return () => this._connectionListeners.delete(handler);
    }

    async connect(options) {
      if (this.isReady) return this;
      if (this._connectPromise) return this._connectPromise;

      const supplied = options || this.loadConfig();
      const config = this._normalizeConfig(supplied);
      this._config = config;
      this._manualDisconnect = false;
      this._authFailed = false;
      this._clearReconnectTimer();
      return this._openConnection(false);
    }

    disconnect(options) {
      const settings = options || {};
      this._manualDisconnect = settings.manual !== false;
      this._clearReconnectTimer();
      this._stopHeartbeat();
      this._rejectAllPending(new HomeAssistantError("连接已关闭。", { code: "connection_closed" }));
      this._clearActiveSubscriptionIds();

      const socket = this._socket;
      this._socket = null;
      this._setStatus(STATUS.CLOSED, { manual: this._manualDisconnect });
      if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) {
        socket.close(1000, "Client disconnected");
      }
    }

    saveConfig(config) {
      const normalized = this._normalizeConfig(config || this._config);
      localStorage.setItem(STORAGE_KEY, JSON.stringify({
        baseUrl: normalized.baseUrl,
        accessToken: normalized.accessToken,
        reconnect: normalized.reconnect
      }));
      return { baseUrl: normalized.baseUrl, reconnect: normalized.reconnect };
    }

    loadConfig() {
      const text = localStorage.getItem(STORAGE_KEY);
      if (!text) return null;
      try {
        const saved = JSON.parse(text);
        return this._normalizeConfig(saved);
      } catch (error) {
        console.warn("Home Assistant 本地配置解析失败，将忽略该配置。", error);
        return null;
      }
    }

    clearConfig() {
      localStorage.removeItem(STORAGE_KEY);
    }

    async sendCommand(message, options) {
      const request = this._sendCommand(message, options);
      return request.promise;
    }

    async getStates() {
      return this.sendCommand({ type: "get_states" });
    }

    async getConfig() {
      return this.sendCommand({ type: "get_config" });
    }

    async getServices() {
      return this.sendCommand({ type: "get_services" });
    }

    async callService(domain, service, options) {
      if (!domain || !service) {
        throw new TypeError("callService 需要 domain 和 service。");
      }
      const settings = options || {};
      const message = {
        type: "call_service",
        domain,
        service,
        return_response: settings.returnResponse !== false
      };
      if (settings.target) message.target = settings.target;
      if (settings.serviceData) message.service_data = settings.serviceData;
      return this.sendCommand(message, { timeout: settings.timeout });
    }

    async subscribeEvents(eventType, handler) {
      if (typeof eventType === "function") {
        handler = eventType;
        eventType = null;
      }
      if (typeof handler !== "function") {
        throw new TypeError("subscribeEvents 的 handler 必须是函数。");
      }

      const subscription = {
        eventType: eventType || null,
        handler,
        activeId: null,
        desired: true
      };
      this._subscriptions.add(subscription);
      try {
        await this._activateSubscription(subscription);
      } catch (error) {
        subscription.desired = false;
        this._subscriptions.delete(subscription);
        throw error;
      }

      return async () => this._unsubscribe(subscription);
    }

    async subscribeStateChanges(handler) {
      if (typeof handler !== "function") {
        throw new TypeError("subscribeStateChanges 的 handler 必须是函数。");
      }
      return this.subscribeEvents("state_changed", (event) => {
        const data = event.data || {};
        handler({
          entityId: data.entity_id || null,
          oldState: data.old_state || null,
          newState: data.new_state || null,
          event
        });
      });
    }

    async watchEntity(entityId, handler) {
      if (!entityId || typeof handler !== "function") {
        throw new TypeError("watchEntity 需要 entityId 和回调函数。");
      }
      return this.subscribeStateChanges((change) => {
        if (change.entityId === entityId) handler(change.newState, change.oldState, change);
      });
    }

    _normalizeConfig(options) {
      if (!options || typeof options !== "object") {
        throw new TypeError("请提供 baseUrl 与 accessToken。\n例如：ha.connect({ baseUrl: 'http://ha.local:8123', accessToken: 'token' })");
      }
      const baseUrl = String(options.baseUrl || "").trim().replace(/\/+$/, "");
      const accessToken = String(options.accessToken || "").trim();
      if (!baseUrl) throw new TypeError("baseUrl 不能为空。");
      if (!accessToken) throw new TypeError("accessToken 不能为空。");

      let parsed;
      try {
        parsed = new URL(baseUrl);
      } catch {
        throw new TypeError("baseUrl 必须是完整 HTTP(S) 地址，例如 http://ha.local:8123。");
      }
      if (parsed.protocol !== "http:" && parsed.protocol !== "https:") {
        throw new TypeError("baseUrl 仅支持 http:// 或 https://。");
      }
      return {
        baseUrl,
        accessToken,
        reconnect: options.reconnect !== false,
        heartbeatInterval: Number(options.heartbeatInterval) || 25000,
        heartbeatTimeout: Number(options.heartbeatTimeout) || 10000,
        requestTimeout: Number(options.requestTimeout) || 20000
      };
    }

    _buildWebSocketUrl(baseUrl) {
      const url = new URL(baseUrl);
      url.protocol = url.protocol === "https:" ? "wss:" : "ws:";
      url.pathname = `${url.pathname.replace(/\/$/, "")}/api/websocket`.replace(/\/\/+/g, "/");
      url.search = "";
      url.hash = "";
      return url.toString();
    }

    _openConnection(isReconnect) {
      if (this._connectPromise) return this._connectPromise;
      if (!this._config) return Promise.reject(new HomeAssistantError("尚未配置连接参数。", { code: "missing_config" }));

      this._setStatus(isReconnect ? STATUS.RECONNECTING : STATUS.CONNECTING, { url: this._config.baseUrl });
      this._nextId = 1;
      this._connectPromise = new Promise((resolve, reject) => {
        this._connectResolve = resolve;
        this._connectReject = reject;
      });

      let socket;
      try {
        socket = new WebSocket(this._buildWebSocketUrl(this._config.baseUrl));
      } catch (error) {
        this._failConnection(error);
        return this._connectPromise;
      }
      this._socket = socket;

      socket.onopen = () => {
        if (socket !== this._socket) return;
        this._setStatus(STATUS.AUTHENTICATING);
      };
      socket.onmessage = (event) => this._handleMessage(socket, event);
      socket.onerror = () => {
        // close 事件会统一执行清理和重连；浏览器通常不给出可用的 error 详情。
      };
      socket.onclose = (event) => this._handleClose(socket, event);
      return this._connectPromise;
    }

    async _handleMessage(socket, messageEvent) {
      if (socket !== this._socket) return;
      let message;
      try {
        message = JSON.parse(messageEvent.data);
      } catch (error) {
        console.warn("收到无法解析的 Home Assistant WebSocket 消息。", error);
        return;
      }

      if (message.type === "auth_required") {
        this._sendRaw({ type: "auth", access_token: this._config.accessToken });
        return;
      }
      if (message.type === "auth_ok") {
        this._serverVersion = message.ha_version || null;
        this._reconnectAttempts = 0;
        try {
          // auth_ok 后协议已可发送命令；先进入 ready，才能复建旧订阅。
          this._setStatus(STATUS.READY, { haVersion: this._serverVersion });
          await this._restoreSubscriptions();
          this._resolveConnection();
          this._startHeartbeat();
        } catch (error) {
          this._failConnection(error);
          socket.close(1011, "Subscription restore failed");
        }
        return;
      }
      if (message.type === "auth_invalid") {
        this._authFailed = true;
        const error = new HomeAssistantError(message.message || "Home Assistant 鉴权失败。", {
          code: "auth_invalid",
          raw: message
        });
        this._setStatus(STATUS.AUTH_FAILED, { message: error.message });
        this._rejectConnection(error);
        socket.close(1008, "Authentication failed");
        return;
      }
      if (message.type === "result") {
        this._handleResult(message);
        return;
      }
      if (message.type === "event") {
        this._handleEvent(message);
        return;
      }
      if (message.type === "pong") {
        this._handlePong(message.id);
        return;
      }
      console.warn("收到未知 Home Assistant WebSocket 消息类型。", message);
    }

    _handleResult(message) {
      const pending = this._pending.get(message.id);
      if (!pending) {
        console.warn("收到未匹配请求的 Home Assistant 结果。", message);
        return;
      }
      this._pending.delete(message.id);
      clearTimeout(pending.timer);
      if (message.success) {
        pending.resolve(message.result);
      } else {
        pending.reject(new HomeAssistantError(
          (message.error && message.error.message) || "Home Assistant 命令执行失败。",
          { code: message.error && message.error.code, raw: message.error || message }
        ));
      }
    }

    _handleEvent(message) {
      const subscription = this._subscriptionsById.get(message.id);
      if (!subscription || !subscription.desired) return;
      try {
        subscription.handler(message.event);
      } catch (error) {
        console.error("Home Assistant 事件订阅回调发生异常。", error);
      }
    }

    _handlePong(id) {
      if (!this._heartbeatPending || this._heartbeatPending.id !== id) return;
      clearTimeout(this._heartbeatPending.timer);
      this._heartbeatPending = null;
    }

    _sendCommand(message, options) {
      if (!this.isReady || !this._socket || this._socket.readyState !== WebSocket.OPEN) {
        throw new HomeAssistantError("WebSocket 尚未连接完成。", { code: "not_ready" });
      }
      if (!message || typeof message !== "object" || !message.type) {
        throw new TypeError("消息必须是包含 type 字段的对象。");
      }
      const id = this._allocateId();
      const request = { ...message, id };
      const timeout = options && options.timeout != null ? Number(options.timeout) : this._config.requestTimeout;
      let resolve;
      let reject;
      const promise = new Promise((resolveFn, rejectFn) => {
        resolve = resolveFn;
        reject = rejectFn;
      });
      const timer = setTimeout(() => {
        if (!this._pending.has(id)) return;
        this._pending.delete(id);
        reject(new HomeAssistantError(`请求超时：${message.type}`, { code: "request_timeout" }));
      }, Math.max(1, timeout));

      this._pending.set(id, { resolve, reject, timer, type: message.type });
      try {
        this._sendRaw(request);
      } catch (error) {
        this._pending.delete(id);
        clearTimeout(timer);
        reject(error);
      }
      return { id, promise };
    }

    _sendRaw(message) {
      if (!this._socket || this._socket.readyState !== WebSocket.OPEN) {
        throw new HomeAssistantError("WebSocket 不可用。", { code: "socket_unavailable" });
      }
      this._socket.send(JSON.stringify(message));
    }

    _allocateId() {
      return this._nextId++;
    }

    async _activateSubscription(subscription) {
      if (!subscription.desired) return;
      const message = { type: "subscribe_events" };
      if (subscription.eventType) message.event_type = subscription.eventType;
      const request = this._sendCommand(message);
      subscription.activeId = request.id;
      this._subscriptionsById.set(request.id, subscription);
      try {
        await request.promise;
      } catch (error) {
        this._subscriptionsById.delete(request.id);
        subscription.activeId = null;
        throw error;
      }
    }

    async _unsubscribe(subscription) {
      if (!subscription.desired) return;
      subscription.desired = false;
      this._subscriptions.delete(subscription);
      const activeId = subscription.activeId;
      subscription.activeId = null;
      if (activeId == null) return;

      if (!this.isReady) {
        this._subscriptionsById.delete(activeId);
        return;
      }
      await this.sendCommand({ type: "unsubscribe_events", subscription: activeId });
      this._subscriptionsById.delete(activeId);
    }

    async _restoreSubscriptions() {
      this._clearActiveSubscriptionIds();
      const subscriptions = [...this._subscriptions].filter((item) => item.desired);
      for (const subscription of subscriptions) {
        await this._activateSubscription(subscription);
      }
    }

    _clearActiveSubscriptionIds() {
      this._subscriptionsById.clear();
      for (const subscription of this._subscriptions) subscription.activeId = null;
    }

    _startHeartbeat() {
      this._stopHeartbeat();
      this._heartbeatTimer = setInterval(() => {
        if (!this.isReady || this._heartbeatPending) return;
        const id = this._allocateId();
        const timer = setTimeout(() => {
          if (!this._heartbeatPending || this._heartbeatPending.id !== id) return;
          this._heartbeatPending = null;
          if (this._socket) this._socket.close(4000, "Heartbeat timeout");
        }, this._config.heartbeatTimeout);
        this._heartbeatPending = { id, timer };
        try {
          this._sendRaw({ id, type: "ping" });
        } catch (error) {
          clearTimeout(timer);
          this._heartbeatPending = null;
        }
      }, this._config.heartbeatInterval);
    }

    _stopHeartbeat() {
      if (this._heartbeatTimer) clearInterval(this._heartbeatTimer);
      this._heartbeatTimer = null;
      if (this._heartbeatPending) clearTimeout(this._heartbeatPending.timer);
      this._heartbeatPending = null;
    }

    _handleClose(socket, event) {
      if (socket !== this._socket && this._socket !== null) return;
      if (socket === this._socket) this._socket = null;
      this._stopHeartbeat();
      this._rejectAllPending(new HomeAssistantError("Home Assistant WebSocket 已断开。", {
        code: "connection_closed",
        raw: { code: event.code, reason: event.reason }
      }));
      this._clearActiveSubscriptionIds();
      this._rejectConnection(new HomeAssistantError("连接在准备完成前已关闭。", {
        code: "connection_closed",
        raw: { code: event.code, reason: event.reason }
      }));

      if (this._manualDisconnect || this._authFailed || !this._config || !this._config.reconnect) {
        if (!this._authFailed) this._setStatus(STATUS.CLOSED, { code: event.code, reason: event.reason });
        return;
      }
      this._setStatus(STATUS.RECONNECTING, { code: event.code, reason: event.reason });
      this._scheduleReconnect();
    }

    _scheduleReconnect() {
      this._clearReconnectTimer();
      const exponent = Math.min(this._reconnectAttempts++, 5);
      const baseDelay = Math.min(1000 * (2 ** exponent), 30000);
      const jitter = Math.round(Math.random() * Math.min(500, baseDelay * 0.2));
      const delay = baseDelay + jitter;
      this._setStatus(STATUS.RECONNECTING, { retryIn: delay, attempt: this._reconnectAttempts });
      this._reconnectTimer = setTimeout(() => {
        this._reconnectTimer = null;
        this._openConnection(true).catch(() => {
          // close 事件会继续安排下一次重连，避免未处理的 Promise 拒绝。
        });
      }, delay);
    }

    _clearReconnectTimer() {
      if (this._reconnectTimer) clearTimeout(this._reconnectTimer);
      this._reconnectTimer = null;
    }

    _rejectAllPending(error) {
      for (const pending of this._pending.values()) {
        clearTimeout(pending.timer);
        pending.reject(error);
      }
      this._pending.clear();
    }

    _resolveConnection() {
      const resolve = this._connectResolve;
      this._connectPromise = null;
      this._connectResolve = null;
      this._connectReject = null;
      if (resolve) resolve(this);
    }

    _rejectConnection(error) {
      const reject = this._connectReject;
      this._connectPromise = null;
      this._connectResolve = null;
      this._connectReject = null;
      if (reject) reject(error);
    }

    _failConnection(error) {
      this._setStatus(STATUS.CLOSED, { error: error.message || String(error) });
      this._rejectConnection(error);
      if (!this._manualDisconnect && !this._authFailed && this._config && this._config.reconnect) {
        this._scheduleReconnect();
      }
    }

    _setStatus(status, detail) {
      const changed = this._status !== status;
      this._status = status;
      if (!changed && !detail) return;
      for (const listener of this._connectionListeners) {
        try {
          listener(status, detail || null);
        } catch (error) {
          console.error("Home Assistant 连接状态回调发生异常。", error);
        }
      }
    }
  }

  global.HomeAssistantWs = HomeAssistantWs;
  global.HomeAssistantError = HomeAssistantError;
  global.ha = new HomeAssistantWs();
})(window);
