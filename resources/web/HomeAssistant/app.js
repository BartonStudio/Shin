(() => {
  "use strict";

  const $ = (id) => document.getElementById(id);
  const ui = {
    baseUrl: $("baseUrl"),
    accessToken: $("accessToken"),
    reconnect: $("reconnect"),
    connect: $("connect"),
    disconnect: $("disconnect"),
    saveConfig: $("saveConfig"),
    clearConfig: $("clearConfig"),
    connectionDot: $("connectionDot"),
    connectionText: $("connectionText"),
    serverVersion: $("serverVersion"),
    notice: $("notice"),
    domain: $("domain"),
    service: $("service"),
    targetEntity: $("targetEntity"),
    serviceData: $("serviceData"),
    callService: $("callService"),
    loadServices: $("loadServices"),
    refreshEntities: $("refreshEntities"),
    entityFilter: $("entityFilter"),
    entityList: $("entityList"),
    entityCount: $("entityCount"),
    selectedEntity: $("selectedEntity"),
    entityDetail: $("entityDetail"),
    eventType: $("eventType"),
    subscribeEvent: $("subscribeEvent"),
    subscribeEntity: $("subscribeEntity"),
    clearSubscriptions: $("clearSubscriptions"),
    subscriptionList: $("subscriptionList"),
    log: $("log"),
    clearLog: $("clearLog")
  };

  let entities = [];
  let selectedEntityId = null;
  let subscriptions = [];

  function format(value) {
    return JSON.stringify(value, null, 2);
  }

  function getErrorPayload(error) {
    if (error instanceof Error) {
      return { name: error.name, code: error.code || null, message: error.message, raw: error.raw || null };
    }
    return error;
  }

  function log(title, payload) {
    const timestamp = new Date().toLocaleTimeString("zh-CN", { hour12: false });
    const previous = ui.log.textContent === "等待操作。" ? "" : `${ui.log.textContent}\n\n`;
    ui.log.textContent = `${previous}[${timestamp}] ${title}\n${format(payload)}`;
    ui.log.scrollTop = ui.log.scrollHeight;
  }

  function showNotice(text, kind = "warn") {
    ui.notice.textContent = text || "";
    ui.notice.className = text ? `notice show ${kind}` : "notice";
  }

  function setBusy(button, busy, busyText) {
    if (!button.dataset.defaultText) button.dataset.defaultText = button.textContent;
    button.disabled = busy;
    button.textContent = busy ? busyText : button.dataset.defaultText;
  }

  function requireReady() {
    if (!window.ha || !ha.isReady) {
      throw new Error("尚未连接到 Home Assistant，请先完成鉴权。");
    }
  }

  function getConfigFromForm() {
    return {
      baseUrl: ui.baseUrl.value.trim(),
      accessToken: ui.accessToken.value.trim(),
      reconnect: ui.reconnect.checked
    };
  }

  function fillConfig() {
    const config = ha.loadConfig();
    if (!config) return;
    ui.baseUrl.value = config.baseUrl || "";
    ui.accessToken.value = config.accessToken || "";
    ui.reconnect.checked = config.reconnect !== false;
  }

  function renderEntities() {
    const keyword = ui.entityFilter.value.trim().toLowerCase();
    const filtered = entities.filter((entity) => {
      if (!keyword) return true;
      const name = entity.attributes && entity.attributes.friendly_name ? entity.attributes.friendly_name : "";
      return [entity.entity_id, name, entity.state].some((item) => String(item || "").toLowerCase().includes(keyword));
    });
    ui.entityCount.textContent = `共 ${entities.length} 个；显示 ${filtered.length} 个`;
    if (!filtered.length) {
      ui.entityList.innerHTML = '<div class="empty">没有符合筛选条件的实体。</div>';
      return;
    }
    ui.entityList.innerHTML = "";
    for (const entity of filtered) {
      const button = document.createElement("button");
      button.type = "button";
      button.className = `entity${entity.entity_id === selectedEntityId ? " selected" : ""}`;
      button.dataset.entityId = entity.entity_id;
      const label = entity.attributes && entity.attributes.friendly_name ? entity.attributes.friendly_name : "未命名实体";
      button.innerHTML = `<span><span class="entity-id">${escapeHtml(entity.entity_id)}</span><span class="entity-name">${escapeHtml(label)}</span></span><span class="state">${escapeHtml(entity.state)}</span>`;
      button.addEventListener("click", () => selectEntity(entity.entity_id));
      ui.entityList.appendChild(button);
    }
  }

  function escapeHtml(value) {
    return String(value == null ? "" : value).replace(/[&<>'"]/g, (char) => ({
      "&": "&amp;", "<": "&lt;", ">": "&gt;", "'": "&#39;", '"': "&quot;"
    })[char]);
  }

  function selectEntity(entityId) {
    const entity = entities.find((item) => item.entity_id === entityId);
    if (!entity) return;
    selectedEntityId = entityId;
    ui.selectedEntity.textContent = entityId;
    ui.entityDetail.textContent = format(entity);
    ui.targetEntity.value = entityId;
    renderEntities();
  }

  async function refreshEntities() {
    requireReady();
    setBusy(ui.refreshEntities, true, "刷新中…");
    try {
      entities = await ha.getStates();
      renderEntities();
      if (selectedEntityId) selectEntity(selectedEntityId);
      log("get_states 返回", { entityCount: entities.length, entities });
      showNotice(`已获取 ${entities.length} 个实体；点击列表可查看完整属性。`, "ok");
    } catch (error) {
      log("get_states 失败", getErrorPayload(error));
      showNotice(`获取实体失败：${error.message || error}`);
    } finally {
      setBusy(ui.refreshEntities, false);
    }
  }

  function parseServiceData() {
    const text = ui.serviceData.value.trim();
    if (!text) return undefined;
    let parsed;
    try {
      parsed = JSON.parse(text);
    } catch (error) {
      throw new Error(`Service Data 不是有效 JSON：${error.message}`);
    }
    if (!parsed || Array.isArray(parsed) || typeof parsed !== "object") {
      throw new Error("Service Data 必须是 JSON 对象。");
    }
    return parsed;
  }

  async function callService() {
    requireReady();
    const domain = ui.domain.value.trim();
    const service = ui.service.value.trim();
    const entityId = ui.targetEntity.value.trim();
    if (!domain || !service) throw new Error("请填写 Domain 和 Service。");
    const serviceData = parseServiceData();
    const options = { returnResponse: true };
    if (entityId) options.target = { entity_id: entityId };
    if (serviceData) options.serviceData = serviceData;

    setBusy(ui.callService, true, "调用中…");
    try {
      const result = await ha.callService(domain, service, options);
      log(`call_service: ${domain}.${service}`, { target: options.target || null, serviceData: serviceData || null, result });
      showNotice(`服务 ${domain}.${service} 已被 Home Assistant 接收。实际状态请刷新实体或查看订阅事件。`, "ok");
    } catch (error) {
      log(`call_service: ${domain}.${service} 失败`, getErrorPayload(error));
      showNotice(`服务调用失败：${error.message || error}`);
    } finally {
      setBusy(ui.callService, false);
    }
  }

  function renderSubscriptions() {
    if (!subscriptions.length) {
      ui.subscriptionList.innerHTML = '<div class="empty">暂未建立订阅。</div>';
      return;
    }
    ui.subscriptionList.innerHTML = "";
    for (const record of subscriptions) {
      const item = document.createElement("div");
      item.className = "sub-item";
      item.innerHTML = `<code>${escapeHtml(record.label)}</code>`;
      const stop = document.createElement("button");
      stop.type = "button";
      stop.className = "secondary";
      stop.textContent = "取消";
      stop.addEventListener("click", () => stopSubscription(record));
      item.appendChild(stop);
      ui.subscriptionList.appendChild(item);
    }
  }

  async function stopSubscription(record) {
    try {
      await record.unsubscribe();
      subscriptions = subscriptions.filter((item) => item !== record);
      renderSubscriptions();
      log("订阅已取消", { subscription: record.label });
    } catch (error) {
      log("取消订阅失败", getErrorPayload(error));
      showNotice(`取消订阅失败：${error.message || error}`);
    }
  }

  async function subscribeEvent() {
    requireReady();
    const eventType = ui.eventType.value.trim() || null;
    setBusy(ui.subscribeEvent, true, "订阅中…");
    try {
      const unsubscribe = await ha.subscribeEvents(eventType, (event) => {
        log(`事件推送：${event.event_type || eventType || "unknown"}`, event);
      });
      subscriptions.push({ label: `事件：${eventType || "全部事件"}`, unsubscribe });
      renderSubscriptions();
      log("已建立事件订阅", { eventType: eventType || null });
      showNotice(`已订阅 ${eventType || "全部事件"}，推送会写入日志。`, "ok");
    } catch (error) {
      log("订阅事件失败", getErrorPayload(error));
      showNotice(`订阅失败：${error.message || error}`);
    } finally {
      setBusy(ui.subscribeEvent, false);
    }
  }

  async function subscribeSelectedEntity() {
    requireReady();
    const entityId = selectedEntityId || ui.targetEntity.value.trim();
    if (!entityId) throw new Error("请先在实体列表选择一个实体，或在目标实体 ID 中填写实体。\n");
    setBusy(ui.subscribeEntity, true, "订阅中…");
    try {
      const unsubscribe = await ha.watchEntity(entityId, (newState, oldState, change) => {
        log(`实体变化：${entityId}`, { entityId, oldState, newState, event: change.event });
        const index = entities.findIndex((item) => item.entity_id === entityId);
        if (newState) {
          if (index >= 0) entities[index] = newState;
          else entities.push(newState);
        } else if (index >= 0) {
          entities.splice(index, 1);
          if (selectedEntityId === entityId) {
            selectedEntityId = null;
            ui.selectedEntity.textContent = "未选择";
            ui.entityDetail.textContent = "该实体已从 Home Assistant 状态列表中移除。";
          }
        }
        if (selectedEntityId === entityId && newState) selectEntity(entityId);
        renderEntities();
      });
      subscriptions.push({ label: `实体：${entityId}`, unsubscribe });
      renderSubscriptions();
      log("已建立实体订阅", { entityId });
      showNotice(`已订阅 ${entityId} 的 state_changed 推送。`, "ok");
    } catch (error) {
      log("订阅实体失败", getErrorPayload(error));
      showNotice(`订阅失败：${error.message || error}`);
    } finally {
      setBusy(ui.subscribeEntity, false);
    }
  }

  async function clearSubscriptions() {
    const records = subscriptions.slice();
    for (const record of records) await stopSubscription(record);
    showNotice("全部本地订阅已取消。", "ok");
  }

  async function connect() {
    setBusy(ui.connect, true, "连接中…");
    try {
      await ha.connect(getConfigFromForm());
      showNotice("已完成 Home Assistant 鉴权连接。", "ok");
      log("连接成功", { baseUrl: ha.config && ha.config.baseUrl, serverVersion: ha.serverVersion });
    } catch (error) {
      log("连接失败", getErrorPayload(error));
      showNotice(`连接失败：${error.message || error}`);
    } finally {
      setBusy(ui.connect, false);
    }
  }

  ha.onConnectionState((status, detail) => {
    const names = {
      idle: "未连接", connecting: "正在连接", authenticating: "正在鉴权", ready: "已连接", reconnecting: "正在重连", closed: "已断开", auth_failed: "鉴权失败"
    };
    ui.connectionText.textContent = names[status] || status;
    ui.connectionDot.className = `dot${status === "ready" ? " ready" : (status === "auth_failed" ? " bad" : "")}`;
    ui.serverVersion.textContent = ha.serverVersion ? `HA ${ha.serverVersion}` : "—";
    if (status === "reconnecting" || status === "auth_failed") log(`连接状态：${names[status] || status}`, detail || {});
  });

  ui.connect.addEventListener("click", connect);
  ui.disconnect.addEventListener("click", () => {
    ha.disconnect();
    showNotice("已手动断开连接。", "ok");
    log("已手动断开", {});
  });
  ui.saveConfig.addEventListener("click", () => {
    try {
      const config = ha.saveConfig(getConfigFromForm());
      showNotice("连接配置已保存至 localStorage。", "ok");
      log("已保存本地配置", config);
    } catch (error) {
      showNotice(`无法保存配置：${error.message || error}`);
    }
  });
  ui.clearConfig.addEventListener("click", () => {
    ha.clearConfig();
    ui.baseUrl.value = "";
    ui.accessToken.value = "";
    ui.reconnect.checked = true;
    showNotice("已清除 localStorage 中的 Home Assistant 配置。", "ok");
    log("已清除本地配置", {});
  });
  ui.refreshEntities.addEventListener("click", () => refreshEntities());
  ui.entityFilter.addEventListener("input", renderEntities);
  ui.callService.addEventListener("click", () => callService().catch((error) => {
    log("服务调用参数错误", getErrorPayload(error));
    showNotice(error.message || String(error));
  }));
  ui.loadServices.addEventListener("click", async () => {
    try {
      requireReady();
      setBusy(ui.loadServices, true, "加载中…");
      const services = await ha.getServices();
      log("get_services 返回", services);
      showNotice("可用服务已写入响应日志。", "ok");
    } catch (error) {
      log("get_services 失败", getErrorPayload(error));
      showNotice(`获取服务失败：${error.message || error}`);
    } finally {
      setBusy(ui.loadServices, false);
    }
  });
  ui.subscribeEvent.addEventListener("click", () => subscribeEvent().catch((error) => {
    log("事件订阅参数错误", getErrorPayload(error));
    showNotice(error.message || String(error));
  }));
  ui.subscribeEntity.addEventListener("click", () => subscribeSelectedEntity().catch((error) => {
    log("实体订阅参数错误", getErrorPayload(error));
    showNotice(error.message || String(error));
  }));
  ui.clearSubscriptions.addEventListener("click", () => clearSubscriptions().catch((error) => {
    log("批量取消订阅失败", getErrorPayload(error));
    showNotice(error.message || String(error));
  }));
  ui.clearLog.addEventListener("click", () => { ui.log.textContent = "等待操作。"; });

  fillConfig();
})();
