/* HomeAssistantWs.js 最小调用示例。先在页面中加载 HomeAssistantWs.js。 */

async function connectHomeAssistant() {
  const saved = ha.loadConfig();
  await ha.connect(saved || {
    baseUrl: "http://homeassistant.local:8123",
    accessToken: "在此填入 Long-Lived Access Token",
    reconnect: true
  });

  // 用户明确允许保存 Token 到 localStorage 时再调用。
  ha.saveConfig({
    baseUrl: "http://homeassistant.local:8123",
    accessToken: "在此填入 Long-Lived Access Token",
    reconnect: true
  });
}

async function loadCurrentStates() {
  const states = await ha.getStates();
  console.log("本次刷新取得实体数：", states.length);
  return states;
}

async function turnOnLivingRoomLight() {
  // 服务执行成功不等于设备一定已改变状态；需要时可手动刷新 loadCurrentStates()，
  // 或使用 subscribeStateChanges() 等待服务端事件。
  const result = await ha.callService("light", "turn_on", {
    target: { entity_id: "light.living_room" },
    serviceData: { brightness_pct: 60 }
  });
  console.log("服务调用结果：", result);
  return result;
}

async function beginStateLog() {
  const unsubscribe = await ha.subscribeStateChanges((change) => {
    console.log("实体变化：", change.entityId, change.oldState, "=>", change.newState);
  });

  // 例如十分钟后取消：
  // setTimeout(() => unsubscribe(), 10 * 60 * 1000);
  return unsubscribe;
}

const stopConnectionMonitor = ha.onConnectionState((status, detail) => {
  console.log("HA 连接状态：", status, detail || "");
});
