(() => {
  const $ = id => document.getElementById(id);
  const sessionInput = $('sessionId');
  const mobileInput = $('mobile');
  const codeInput = $('smsCode');
  const notice = $('notice');
  const result = $('result');
  const resultMeta = $('resultMeta');
  const sessionStatus = $('sessionStatus');
  const sessionShort = $('sessionShort');
  const transportBadge = $('transportBadge');
  const smsHint = $('smsHint');

  function getMessage(value) {
    if (value && typeof value === 'object') return value.message || value.msg || value.error || '';
    return String(value || '');
  }

  function showResult(label, value, isError = false) {
    const now = new Date().toLocaleTimeString('zh-CN', { hour12: false });
    resultMeta.textContent = `${now} · ${label}${isError ? ' · 请求异常' : ''}`;
    const payload = value instanceof Error
      ? { name: value.name, message: value.message, stack: value.stack }
      : value;
    result.textContent = JSON.stringify(payload, null, 2);
  }

  function setNotice(text = '', type = 'warn') {
    notice.textContent = text;
    notice.className = text ? `notice show ${type}` : 'notice';
  }

  function refreshSession() {
    const id = window.qinlin && qinlin.sessionId ? qinlin.sessionId : '';
    sessionInput.value = id;
    const saved = Boolean(id);
    sessionStatus.innerHTML = `<span><i class="status-dot ${saved ? 'ok' : ''}"></i>${saved ? '已保存 SessionId' : '未保存 SessionId'}</span>`;
    sessionShort.textContent = saved ? `${id.slice(0, 12)}…${id.slice(-6)}` : '—';
  }

  function validMobile() {
    const mobile = mobileInput.value.trim();
    if (!/^1\d{10}$/.test(mobile)) {
      setNotice('请先输入 11 位手机号。');
      mobileInput.focus();
      return null;
    }
    return mobile;
  }

  function setBusy(button, busy, text) {
    if (!button.dataset.defaultText) button.dataset.defaultText = button.textContent;
    button.disabled = busy;
    button.textContent = busy ? text : button.dataset.defaultText;
  }

  // 默认直接 Fetch。若宿主后续实现 HTTP 代发，可替换 qinlin._transport，业务调用无需改变。
  function configureTransport() {
    if (!window.qinlin) {
      setNotice('QinLinApi.js 未成功加载，请确认相对路径。');
      return;
    }
    transportBadge.textContent = '传输层：Fetch';
  }

  $('saveSession').addEventListener('click', () => {
    const id = sessionInput.value.trim();
    if (id && !/^app:[0-9a-f]{32}$/i.test(id)) {
      setNotice('SessionId 格式应为 app: 加 32 位十六进制字符。');
      sessionInput.focus();
      return;
    }
    qinlin.setSession(id);
    refreshSession();
    setNotice(id ? 'SessionId 已保存。' : 'SessionId 已清除。', 'ok');
    showResult('保存 SessionId', { ok: true, sessionId: id || null });
  });

  $('clearSession').addEventListener('click', () => {
    qinlin.setSession('');
    refreshSession();
    setNotice('SessionId 已从浏览器本地存储清除。');
    showResult('清除 SessionId', { ok: true, sessionId: null });
  });

  $('sendSms').addEventListener('click', async event => {
    const mobile = validMobile();
    if (!mobile) return;
    const button = event.currentTarget;
    try {
      setBusy(button, true, '正在发送…');
      setNotice('');
      const response = await qinlin.sendSmsCode(mobile);
      const success = response && (response.code === 0 || response.code === 200);
      showResult('获取手机验证码', response, !success);
      setNotice(success ? '验证码请求已提交，请查看手机短信。' : `验证码请求未成功：${getMessage(response) || '请查看返回值。'}`, success ? 'ok' : 'warn');
      if (success) {
        smsHint.textContent = '验证码已请求；输入短信验证码后点击“登录并保存 SessionId”。';
        codeInput.focus();
      }
    } catch (error) {
      showResult('获取手机验证码', error, true);
      setNotice(`短信请求失败：${error.message}`);
    } finally {
      setBusy(button, false);
    }
  });

  $('login').addEventListener('click', async event => {
    const mobile = validMobile();
    if (!mobile) return;
    const smsCode = codeInput.value.trim();
    if (!smsCode) {
      setNotice('请输入短信验证码。');
      codeInput.focus();
      return;
    }
    const button = event.currentTarget;
    try {
      setBusy(button, true, '正在登录…');
      setNotice('');
      const response = await qinlin.login(mobile, smsCode);
      refreshSession();
      const success = qinlin.isLoggedIn;
      showResult('登录并保存 SessionId', { ok: success, sessionId: qinlin.sessionId || null, resp: response }, !success);
      setNotice(success ? '登录成功，SessionId 已自动保存。' : `登录未取得 SessionId：${getMessage(response) || '请查看返回值。'}`, success ? 'ok' : 'warn');
    } catch (error) {
      showResult('登录并保存 SessionId', error, true);
      setNotice(`登录请求失败：${error.message}`);
    } finally {
      setBusy(button, false);
    }
  });

  document.querySelectorAll('[data-door]').forEach(button => {
    button.addEventListener('click', async () => {
      if (!qinlin.isLoggedIn) {
        setNotice('尚未保存 SessionId，请先粘贴保存或完成短信登录。');
        sessionInput.focus();
        return;
      }
      const door = button.dataset.door;
      try {
        setBusy(button, true, '请求中…');
        setNotice('');
        const response = await qinlin.openDoor(door);
        showResult(`开门：${button.dataset.defaultText}`, response, !response.ok);
        setNotice(response.ok ? '开门请求成功：openDoorState 为 1。' : `开门未成功：${getMessage(response.resp) || '请查看完整返回值。'}`, response.ok ? 'ok' : 'warn');
      } catch (error) {
        showResult(`开门：${button.dataset.defaultText}`, error, true);
        setNotice(`开门请求失败：${error.message}`);
      } finally {
        setBusy(button, false);
      }
    });
  });

  $('clearLog').addEventListener('click', () => {
    resultMeta.textContent = '等待操作';
    result.innerHTML = '<span class="empty">操作后的成功响应、失败响应和异常信息都会原样显示在这里。</span>';
  });

  mobileInput.addEventListener('input', () => { mobileInput.value = mobileInput.value.replace(/\D/g, '').slice(0, 11); });
  codeInput.addEventListener('input', () => { codeInput.value = codeInput.value.replace(/\s/g, ''); });

  configureTransport();
  refreshSession();
})();
