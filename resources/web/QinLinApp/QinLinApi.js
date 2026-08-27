/**
 * QinLinApi.js
 * 亲邻开门 API 客户端（短信 -> 登录 -> 开门）
 * 对应文档：resources/web/QinLinApp/亲邻开门-接口文档.md
 *
 * 分层设计：
 *   第1层 纯函数加密工具  md5 / aesEcbEncryptHex   （无状态、可单测）
 *   第2层 签名构造        buildSign               （字典序拼接 + 盐 + MD5大写）
 *   第3层 传输层          QinLinApi._request      （可注入：fetch 或走 ShinBridge 让 C++ 发）
 *   第4层 业务接口        sendSmsCode / login / openDoor
 *   第5层 会话管理        sessionId 自动保存/读取/清除（localStorage）
 *
 * 注意：WebView2 里直接 fetch 外部域名可能遇到 CORS，
 * 此时把 _transport 换成 (url, opts) => shin.send('http', {url, ...opts}) 由 C++ 代发即可。
 */

// ============================== 第0层 常量 ==============================

const QINLIN_CONFIG = {
    HOST_SMS: 'https://gateway2.qinlinkeji.com',
    HOST_API: 'https://mobileapi3.qinlinkeji.com',

    APP_ID: 'gbDjIZQSOpCMX49P',
    APP_SECRET: 'OKFoQ9MmNXQtcyXROo4PnaFkfDPTHuDg',   // 短信 header 签名盐
    SIGN_KEY: 'qiAnlPinP',                            // body/query 签名盐
    AES_KEY_HEX: 'FBC213C4C7BEEBD2AA4EDBF0F681C41B', // 手机号 AES-128-ECB

    APP_VERSION: '5.2.1',
    COMMUNITY_ID: '4222',

    COMMON_HEADERS: {
        'user-agent': 'Dart/3.10 (dart:io)',
        'qversioncode': '3141',
        'qchannel': 'xiaomi',
        'qplatform': '0',
        'qvendor': 'OnePlus',
        'content-type': 'application/json; charset=utf-8',
    },

    // 门编号对照（文档第3节）
    DOORS: {
        lobby: '16708',   // D10 大堂门
        garage: '16709',  // D10 负一 / 地库门
        slide: '16671',   // D12 手动推拉门
    },
};

// ============================== 第1层 加密工具 ==============================

/** MD5（纯 JS，WebCrypto 不支持 MD5）。返回 32 位大写 hex。 */
function md5Upper(input) {
    const bytes = new TextEncoder().encode(input);
    const bitLen = bytes.length * 8;
    const rem = bytes.length % 64;
    const padLen = (rem < 56) ? (56 - rem) : (120 - rem);
    const msg = new Uint8Array(bytes.length + padLen + 8);
    msg.set(bytes);
    msg[bytes.length] = 0x80;
    const dv = new DataView(msg.buffer);
    dv.setUint32(msg.length - 8, bitLen >>> 0, true);
    dv.setUint32(msg.length - 4, Math.floor(bitLen / 4294967296), true);

    let a0 = 0x67452301, b0 = 0xefcdab89 | 0, c0 = 0x98badcfe | 0, d0 = 0x10325476;
    const S = [7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
               5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
               4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
               6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21];
    const K = new Int32Array(64);
    for (let i = 0; i < 64; i++) K[i] = Math.floor(Math.abs(Math.sin(i + 1)) * 4294967296);
    const rol = (x, n) => (x << n) | (x >>> (32 - n));

    for (let off = 0; off < msg.length; off += 64) {
        const M = new Int32Array(16);
        for (let i = 0; i < 16; i++) M[i] = dv.getUint32(off + i * 4, true);
        let A = a0, B = b0, C = c0, D = d0;
        for (let i = 0; i < 64; i++) {
            let F, g;
            if (i < 16)      { F = (B & C) | (~B & D); g = i; }
            else if (i < 32) { F = (D & B) | (~D & C); g = (5 * i + 1) % 16; }
            else if (i < 48) { F = B ^ C ^ D;          g = (3 * i + 5) % 16; }
            else             { F = C ^ (B | ~D);       g = (7 * i) % 16; }
            const tmp = D;
            D = C; C = B;
            B = (B + rol((A + F + K[i] + M[g]) | 0, S[i])) | 0;
            A = tmp;
        }
        a0 = (a0 + A) | 0; b0 = (b0 + B) | 0; c0 = (c0 + C) | 0; d0 = (d0 + D) | 0;
    }
    const out = new Uint8Array(16);
    const odv = new DataView(out.buffer);
    odv.setUint32(0, a0, true); odv.setUint32(4, b0, true);
    odv.setUint32(8, c0, true); odv.setUint32(12, d0, true);
    return [...out].map(b => b.toString(16).padStart(2, '0')).join('').toUpperCase();
}

/** AES-128-ECB + PKCS7 加密（仅加密，够用即可）。key 为 16 字节，返回大写 hex。 */
const _aes = (() => {
    const SBOX = [
        0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
        0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
        0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
        0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
        0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
        0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
        0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
        0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
        0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
        0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
        0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
        0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
        0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
        0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
        0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
        0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16,
    ];
    const RCON = [0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36];
    const xt = x => ((x << 1) ^ ((x & 0x80) ? 0x1b : 0)) & 0xff;

    function expandKey(key) { // 16 -> 176 字节
        const w = new Uint8Array(176);
        w.set(key);
        let bytesGen = 16, rconIdx = 0;
        const t = new Uint8Array(4);
        while (bytesGen < 176) {
            for (let i = 0; i < 4; i++) t[i] = w[bytesGen - 4 + i];
            if (bytesGen % 16 === 0) {
                const first = t[0];                       // RotWord
                t[0] = t[1]; t[1] = t[2]; t[2] = t[3]; t[3] = first;
                for (let i = 0; i < 4; i++) t[i] = SBOX[t[i]]; // SubWord
                t[0] ^= RCON[rconIdx++];
            }
            for (let i = 0; i < 4; i++) { w[bytesGen] = w[bytesGen - 16] ^ t[i]; bytesGen++; }
        }
        return w;
    }

    function encryptBlock(state, w) { // state: Uint8Array(16)，列主序
        const addRoundKey = r => { for (let i = 0; i < 16; i++) state[i] ^= w[r * 16 + i]; };
        addRoundKey(0);
        for (let round = 1; round <= 10; round++) {
            for (let i = 0; i < 16; i++) state[i] = SBOX[state[i]];          // SubBytes
            // ShiftRows（列主序 state[r + 4c]）
            for (let r = 1; r < 4; r++) {
                const row = [state[r], state[r + 4], state[r + 8], state[r + 12]];
                for (let c = 0; c < 4; c++) state[r + 4 * c] = row[(c + r) % 4];
            }
            if (round < 10) {                                                 // MixColumns
                for (let c = 0; c < 4; c++) {
                    const i = 4 * c;
                    const a = state[i], b = state[i + 1], cc = state[i + 2], d = state[i + 3];
                    state[i]     = xt(a) ^ (xt(b) ^ b) ^ cc ^ d;
                    state[i + 1] = a ^ xt(b) ^ (xt(cc) ^ cc) ^ d;
                    state[i + 2] = a ^ b ^ xt(cc) ^ (xt(d) ^ d);
                    state[i + 3] = (xt(a) ^ a) ^ b ^ cc ^ xt(d);
                }
            }
            addRoundKey(round);
        }
        return state;
    }

    return { expandKey, encryptBlock };
})();

function aesEcbEncryptHex(plain) {
    const keyHex = QINLIN_CONFIG.AES_KEY_HEX;
    const key = new Uint8Array(16);
    for (let i = 0; i < 16; i++) key[i] = parseInt(keyHex.substr(i * 2, 2), 16);
    const data = new TextEncoder().encode(plain);
    const pad = 16 - (data.length % 16);
    const padded = new Uint8Array(data.length + pad);
    padded.set(data);
    padded.fill(pad, data.length);
    const w = _aes.expandKey(key);
    const out = [];
    for (let off = 0; off < padded.length; off += 16) {
        out.push(..._aes.encryptBlock(padded.slice(off, off + 16), w));
    }
    return out.map(b => b.toString(16).padStart(2, '0')).join('').toUpperCase();
}

// ============================== 第2层 签名构造 ==============================

/**
 * 通用签名：参数全部转字符串 -> key 字典序拼接 k=v& -> 末尾加盐 -> MD5 大写
 * @param {object} params 参与签名的字段（不含 sign 自身）
 * @param {string} saltPair 末尾盐对，如 "key=qiAnlPinP" 或 "appsecret=xxx"
 */
function buildSign(params, saltPair) {
    const raw = Object.keys(params).sort()
        .map(k => `${k}=${String(params[k])}`)
        .join('&') + `&${saltPair}`;
    return md5Upper(raw);
}

const _nonce = digits => String(Math.floor(Math.random() * 10 ** digits)).padStart(digits, '0');
const _ts = () => String(Date.now());

// ============================== 第3~5层 API 客户端 ==============================

class QinLinApi {
    constructor() {
        this._storageKey = 'qinlin_sessionId';
        this.sessionId = localStorage.getItem(this._storageKey) || '';
        /**
         * 可替换的传输层：默认 fetch。
         * 若 WebView2 下 CORS 受阻，改为：
         *   qinlin._transport = (url, opts) => shin.send('http', { url, ...opts }).then(r => JSON.parse(r));
         */
        this._transport = async (url, opts = {}) => {
            const resp = await fetch(url, opts);
            return resp.json();
        };
    }

    get isLoggedIn() { return !!this.sessionId; }

    setSession(id) {
        this.sessionId = id || '';
        id ? localStorage.setItem(this._storageKey, id) : localStorage.removeItem(this._storageKey);
    }

    async _request(url, { method = 'POST', query, body, headers } = {}) {
        if (query) url += '?' + new URLSearchParams(query).toString();
        return this._transport(url, {
            method,
            headers: { ...QINLIN_CONFIG.COMMON_HEADERS, ...headers },
            body: body ? JSON.stringify(body) : undefined,
        });
    }

    /** 1. 发送短信验证码（header 签名 + appsecret 盐） */
    async sendSmsCode(mobile) {
        const h = {
            appid: QINLIN_CONFIG.APP_ID,
            nonce: _nonce(4),
            timestamp: _ts(),
            version: 'v2',
        };
        h.sign = buildSign({ ...h, mobile }, `appsecret=${QINLIN_CONFIG.APP_SECRET}`);
        return this._request(`${QINLIN_CONFIG.HOST_SMS}/member/sms/sendSecurityCode`, {
            body: { mobile },
            headers: h,
        });
    }

    /** 2. 手机号 + 验证码登录，成功自动保存 sessionId */
    async login(mobile, smsCode) {
        const body = {
            mobile: aesEcbEncryptHex(mobile),
            smsCode,
            appChannel: 2,
            timestamp: _ts(),
            version: QINLIN_CONFIG.APP_VERSION,
            nonce: _nonce(5),
        };
        body.sign = buildSign(body, `key=${QINLIN_CONFIG.SIGN_KEY}`);
        const resp = await this._request(`${QINLIN_CONFIG.HOST_API}/api/app/v1/login`, { body });

        // 响应中 sessionId 位置不固定，直接从整个 JSON 文本里捞
        const m = JSON.stringify(resp).match(/app:[0-9a-f]{32}/i);
        if (m) this.setSession(m[0]);
        return resp;
    }

    /**
     * 3. 开门
     * @param {string} door 'lobby' | 'garage' | 'slide'，或直接传 doorControlId
     * @returns {Promise<{ok:boolean, resp:object}>} ok = 门真的开了
     */
    async openDoor(door = 'lobby') {
        if (!this.isLoggedIn) throw new Error('未登录：请先 login()');
        const doorId = QINLIN_CONFIG.DOORS[door] || door;
        const timestamp = _ts(), nonce = _nonce(5);

        // 关键坑：签名里用 token=sessionId，但 URL 上叫 sessionId，不出现 token
        const sign = buildSign({
            appChannel: 1,
            communityId: QINLIN_CONFIG.COMMUNITY_ID,
            doorControlId: doorId,
            nonce,
            timestamp,
            token: this.sessionId,
            version: QINLIN_CONFIG.APP_VERSION,
        }, `key=${QINLIN_CONFIG.SIGN_KEY}`);

        const resp = await this._request(`${QINLIN_CONFIG.HOST_API}/api/open/doorcontrol/v2/open`, {
            query: {
                sessionId: this.sessionId,
                appChannel: '1',
                doorControlId: doorId,
                communityId: QINLIN_CONFIG.COMMUNITY_ID,
                timestamp, version: QINLIN_CONFIG.APP_VERSION, nonce, sign,
            },
        });

        // 判定：code==0 不够，必须 data.openDoorState==1 才是真开（sign 错时可能 code:0 但 state:6）
        const ok = resp && resp.code === 0 && resp.data && resp.data.openDoorState === 1;
        return { ok, resp };
    }
}

// 全局单例
window.qinlin = new QinLinApi();
