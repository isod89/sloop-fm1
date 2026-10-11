// SPDX-License-Identifier: GPL-3.0-only
// SLOOP editor protocol (web/EDITOR_PROTOCOL.md): the frames, a one-request-at-a-time link, and the
// replies SLOOP LIVE reads. Transport-agnostic: a Link sends raw MIDI bytes through any send(bytes).

export const CMD = {
  INFO: 1, GET: 2, SET: 3, DESC: 5, PRESET: 8, NAMES: 10, UP_LIST: 16, UP_STORE: 19, UP_LOAD: 20,
  WATCH: 22, CHANGED: 23, RELOAD: 24, PING: 25, STEP_CHANGED: 26,
  TRACK: 27, TRACK_MIX: 28, TRACK_DUMP: 29, TRACK_PARAM: 31, TRACK_CHANGED: 32, PERFORM: 43,
};
/* DESC formats (firmware core.h) */
export const F = { INT: 0, PCT: 1, BIPCT: 2, TIME: 3, LFOHZ: 4, CUTOFF: 5, DB: 6, SEMI: 7, ENUM: 8, BPM: 9, NOTE: 10,
  ONOFF: 11, OCT: 12, STEPS: 13, SWING: 14, FILT: 15 };
export const PF = {
  STATE: 0, FX: 1, SECTION: 2, CHAIN: 3, MUTE: 4, SOLO: 5, TRANSPORT: 6, FILL: 7, TAP: 8,
  SONGMODE: 9, SONGREC: 10, STORE: 11,
};
export const NONE = 127;
export const FX_NAMES = ["loop 4", "loop 8", "loop 16", "loop 32", "stutter", "reverse", "stop", "half",
  "low", "high", "phone", "crush", "alias", "gate", "echo", "wobble"];
const PUSH = new Set([CMD.CHANGED, CMD.RELOAD, CMD.STEP_CHANGED, CMD.TRACK_CHANGED]);
const HDR = [0x7d, 0x46, 0x4c];

export const v14 = (v) => { const u = Math.max(-8192, Math.min(8191, v | 0)) + 8192; return [u & 127, u >> 7]; };
export const frame = (cmd, args = []) => Uint8Array.from([0xf0, ...HDR, cmd, ...args.map((b) => b & 127), 0xf7]);

export function unframe(d) {
  if (d.length < 6 || d[0] !== 0xf0 || d[d.length - 1] !== 0xf7) return null;
  if (d[1] !== HDR[0] || d[2] !== HDR[1] || d[3] !== HDR[2]) return null;
  return { cmd: d[4], a: Array.from(d.subarray(5, d.length - 1)) };
}

export class Reader {
  constructor(a) { this.a = a; this.i = 0; }
  get more() { return this.i < this.a.length; }
  b() { return this.a[this.i++] ?? 0; }
  v() { const lo = this.b(), hi = this.b(); return (lo | (hi << 7)) - 8192; }
  s() { let r = ""; while (this.i < this.a.length) { const c = this.a[this.i++]; if (!c) break; r += String.fromCharCode(c); } return r; }
}

/* replies that say which request they answer */
function matches(cmd, args, a) {
  switch (cmd) {
    case CMD.GET: case CMD.SET: case CMD.DESC: return a[0] === args[0] && a[1] === args[1];
    case CMD.TRACK_MIX: case CMD.TRACK_PARAM: case CMD.TRACK_DUMP: case CMD.NAMES: case CMD.UP_STORE:
    case CMD.UP_LOAD: return a[0] === args[0];
    case CMD.UP_LIST: return a[0] === args[0];
    case CMD.PERFORM: return a[0] === (args.length ? args[0] : PF.STATE);
    default: return true;
  }
}

/* One frame in flight (the device holds one SysEx frame), the rest queued. Pushes can arrive any time. */
export class Link {
  constructor(send, { onPush = () => {}, onTimeout = () => {} } = {}) {
    this.send = send; this.onPush = onPush; this.onTimeout = onTimeout;
    this.q = []; this.cur = null; this.closed = false; this.lastSent = 0;
  }
  /* opt: key (a queued request with the same key is replaced: the latest wins), front (jump the queue:
     the pads), timeout, retries */
  request(cmd, args = [], opt = {}) {
    if (this.closed) return Promise.reject(new Error("closed"));
    if (opt.key) {
      const old = this.q.find((x) => x.key === opt.key);
      if (old) { old.cmd = cmd; old.args = args; return old.promise; }
    }
    let res, rej;
    const promise = new Promise((a, b) => { res = a; rej = b; });
    const it = { cmd, args, key: opt.key, res, rej, promise, timeout: opt.timeout ?? 250, retries: opt.retries ?? 1 };
    if (opt.front) {                                   /* after other front items, before the rest */
      const i = this.q.findIndex((x) => !x.front);
      it.front = true;
      this.q.splice(i < 0 ? this.q.length : i, 0, it);
    } else this.q.push(it);
    this.pump();
    return promise;
  }
  pump() { if (!this.cur && !this.closed && this.q.length) { this.cur = this.q.shift(); this.fire(); } }
  fire() {
    const c = this.cur;
    this.lastSent = performance.now();
    try { this.send(frame(c.cmd, c.args)); } catch (e) { this.finish(null, e); return; }
    c.timer = setTimeout(() => {
      if (this.cur !== c) return;
      if (c.retries-- > 0) { this.fire(); return; }
      this.onTimeout(c.cmd);
      this.finish(null, new Error("timeout (cmd " + c.cmd + ")"));
    }, c.timeout);
  }
  finish(val, err) {
    const c = this.cur;
    if (!c) return;
    clearTimeout(c.timer);
    this.cur = null;
    if (err) c.rej(err); else c.res(val);
    this.pump();
  }
  receive(data) {
    const f = unframe(data);
    if (!f) return;
    if (PUSH.has(f.cmd)) { this.onPush(f); return; }
    const c = this.cur;
    if (c && f.cmd === c.cmd && matches(c.cmd, c.args, f.a)) this.finish(f.a);
  }
  close() {
    this.closed = true;
    const all = (this.cur ? [this.cur] : []).concat(this.q);
    if (this.cur) clearTimeout(this.cur.timer);
    this.cur = null; this.q = [];
    for (const r of all) r.rej(new Error("closed"));
  }
}

export function parseInfo(a) {
  const r = new Reader(a);
  const version = r.s(), nEng = r.b(), pCount = r.b(), gCount = r.b(), nStep = r.b(), pE0 = r.b();
  const engines = [];
  for (let i = 0; i < nEng; i++) engines.push(r.s());
  const nTrk = r.more ? r.b() : 1;
  const proto = r.more ? r.b() : 2;
  return { version, nEng, pCount, gCount, nStep, pE0, engines, nTrk, proto };
}

export function parseDesc(a) {
  const r = new Reader(a);
  const scope = r.b(), id = r.b(), fmt = r.b(), min = r.v(), max = r.v(), def = r.v(), label = r.s(), unit = r.s();
  const names = [];
  if (fmt === 8) while (r.more) names.push(r.s());
  return { scope, id, fmt, min, max, def, label, unit, names };
}

export function parseTrack(a) {
  const r = new Reader(a);
  const sel = r.b(), n = r.b(), tracks = [];
  for (let i = 0; i < n; i++) tracks.push({ eng: r.b(), preset: r.b(), level: r.v(), mute: !!r.b(), armed: !!r.b() });
  const solo = r.more ? r.b() : 0;
  return { sel, tracks, solo };
}

/* PERFORM reply: op, rc, then the live state (EDITOR_PROTOCOL.md v11) */
export function parsePerform(a) {
  const r = new Reader(a);
  const op = r.b(), rc = r.b(), flags = r.b(), bpm = r.v(), beat = r.b(), fx = r.b(), mute = r.b(), solo = r.b(),
    armed = r.b(), ready = r.b(), section = r.b(), next = r.b(), songrec = r.b(), chainN = r.b(), chainI = r.b();
  const chain = [];
  for (let i = 0; i < chainN; i++) chain.push(r.b());
  const sel = r.more ? r.b() : -1;                   /* (the selected track and the section lengths: SLOOP live 2) */
  const bars = r.more ? [r.b(), r.b(), r.b(), r.b()] : null;
  return {
    op, rc, bpm, beat, fx: fx === NONE ? -1 : fx, mute, solo, armed, ready, sel, bars,
    section: section === NONE ? -1 : section, next: next === NONE ? -1 : next, songrec, chain, chainI,
    playing: !!(flags & 1), songMode: !!(flags & 2), songPlays: !!(flags & 4), fillHeld: !!(flags & 8),
    fillNext: !!(flags & 16), fxRemote: !!(flags & 32), recArmed: !!(flags & 64),
  };
}

/* TRACK_DUMP: track, engine byte, preset, P_COUNT values */
export function parseDump(a, pCount) {
  const r = new Reader(a);
  const track = r.b(), eng = r.b(), preset = r.b(), p = [];
  for (let i = 0; i < pCount && r.more; i++) p.push(r.v());
  return { track, eng, preset, p };
}

/* NAMES: engine, count, the preset names, then the two edit-page titles */
export function parseNames(a) {
  const r = new Reader(a);
  const eng = r.b(), n = r.b(), names = [];
  for (let i = 0; i < n; i++) names.push(r.s());
  const pages = [r.s(), r.s()];
  return { eng, names, pages };
}

/* UP_LIST: start, count, total, then per slot: used, engine, name */
export function parseUpList(a) {
  const r = new Reader(a);
  const start = r.b(), count = r.b(), total = r.b(), slots = [];
  for (let i = 0; i < count; i++) slots.push({ slot: start + i, used: !!r.b(), eng: r.b(), name: r.s() });
  return { start, total, slots };
}

const NOTE = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
const pct = (v, m) => Math.round(v * 100 / (m || 1));
/* a value as the device would say it (close enough: exact formatting is the device's) */
export function fmtValue(d, v) {
  if (!d) return String(v);
  switch (d.fmt) {
    case F.PCT: return pct(v, d.max) + "%";
    case F.BIPCT: return (v > 0 ? "+" : "") + (v < 0 ? -pct(-v, -d.min) : pct(v, d.max)) + "%";
    case F.ENUM: return (d.names[v - d.min] ?? String(v)).toLowerCase();
    case F.SEMI: return (v > 0 ? "+" : "") + v + " st";
    case F.NOTE: return NOTE[((v % 12) + 12) % 12];
    case F.ONOFF: return v ? "on" : "off";
    case F.OCT: return (v > 0 ? "+" : "") + v + " oct";
    case F.STEPS: return v + " steps";
    case F.SWING: { const s = 50 + v / 4; return (Number.isInteger(s) ? s : s.toFixed(1)) + "%"; }
    case F.FILT: return v === 0 ? "off" : v < 0 ? "low " + pct(-v, 64) + "%" : "high " + pct(v, 63) + "%";
    case F.BPM: return v + " bpm";
    default: return v + (d.unit ? " " + d.unit : "");
  }
}
