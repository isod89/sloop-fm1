// SPDX-License-Identifier: GPL-3.0-only
// The FM-1 as the page sees it: one connection, what it has (INFO, the parameter descriptors), the live state
// (PERFORM STATE ~8 times a second), the four tracks and the selected track's sound. Every change is an event:
//   "connection" {status: "connected" | "lost" | "idle"}   "state"   "tracks"   "sound"   "globals"
import { CMD, PF, NONE, Link, Reader, v14, parseInfo, parseDesc, parseTrack, parsePerform, parseDump, parseNames,
  parseUpList } from "./proto.js";

const POLL_MS = 120;
/* the track parameters SLOOP live shows, by the label the device gives each id (ids move between versions:
   a label that does not match hides its control) */
export const TP_IDS = { LVL: 0, ATK: 1, DEC: 2, SUS: 3, REL: 4, EFLT: 5, EPIT: 6, LRATE: 9, LWAVE: 10, LPHS: 11, LFADE: 12,
  LPIT: 13, LFLT: 14, LSHP: 15, LAMP: 16, AMODE: 17, ARATE: 18, AOCT: 19, AGATE: 20, ASWG: 21, APROB: 22, AHOLD: 23, AORD: 24,
  SSWG: 31, DST: 33, CHO: 34, DLY: 35, REV: 36, GLD: 38, PAN: 39, MUTE: 40, DTUNE: 44, SLCR: 45, SLPAT: 46, SLRATE: 47,
  SLDEPTH: 48, TFLT: 50 };
const TP_LABEL = { LVL: "LVL", ATK: "ATK", DEC: "DEC", SUS: "SUS", REL: "REL", EFLT: "FLT", EPIT: "PIT", LRATE: "RATE",
  LWAVE: "WAVE", LPHS: "PHS", LFADE: "FADE", LPIT: "PIT", LFLT: "FLT", LSHP: "SHP", LAMP: "AMP", AMODE: "MODE", ARATE: "RATE",
  AOCT: "OCT", AGATE: "GATE", ASWG: "SWG", APROB: "PROB", AHOLD: "HOLD", AORD: "ORD", SSWG: "SWG", DST: "DST", CHO: "CHO",
  DLY: "DLY", REV: "REV", GLD: "GLD", PAN: "PAN", MUTE: "MUTE", DTUNE: "DTUNE", SLCR: "SLCR", SLPAT: "PAT", SLRATE: "RATE",
  SLDEPTH: "DEPTH", TFLT: "FILT" };
const GLOBALS = ["BPM", "SWING", "DUST", "DUCK", "FILT", "ROLL", "LVL", "REV", "DLY", "ENG"];

export class Device extends EventTarget {
  constructor() {
    super();
    this.status = "idle";
    this.link = null;
  }
  emit(type, detail) { this.dispatchEvent(new CustomEvent(type, { detail })); }
  get connected() { return this.status === "connected"; }
  get perform() { return !!(this.info && this.info.proto >= 11); }
  get drumEng() { return this.info ? this.info.nEng : 0; }
  isDrum(t) { return !!this.tracks[t] && this.tracks[t].eng >= this.drumEng; }

  async connect(transport) {
    this.disconnect("idle");
    this.transport = transport;
    let link;
    await transport.open((d) => link && link.receive(d), () => this.disconnect("lost"));
    link = this.link = new Link((d) => transport.send(d), { onPush: (f) => this.onPush(f) });
    try {
      const info = parseInfo(await link.request(CMD.INFO, [], { timeout: 600, retries: 3 }));
      this.info = info;
      this.g = {};                                       /* label -> {id, desc, value} */
      for (let id = 0; id < info.gCount; id++) {
        const d = parseDesc(await link.request(CMD.DESC, [1, id]));
        if (!GLOBALS.includes(d.label) || this.g[d.label]) continue;
        const r = new Reader(await link.request(CMD.GET, [1, id]));
        r.b(); r.b();
        this.g[d.label] = { id, desc: d, value: r.v() };
      }
      this.tdesc = {};                                   /* id -> desc (track parameters below P_E0) */
      this.engDesc = {};                                 /* engine -> [8 descs] (P_E0..P_E7) */
      this.names = {};                                   /* engine -> {names, pages} */
      this.live = null;
      await this.readTracks();
      await link.request(CMD.WATCH, [3], { timeout: 400 }).catch(() => {});
      this.status = "connected";
      this.emit("connection", { status: "connected" });
      this.emit("globals");
      this.poll();
      await this.loadSound();
      await this.prefetchNames();
    } catch (e) {
      this.disconnect("idle");
      throw e;
    }
  }

  disconnect(status = "idle") {
    clearTimeout(this.pollT);
    if (this.link) this.link.close();
    this.link = null;
    if (this.transport) { try { this.transport.close(); } catch (_) {} }
    this.transport = null;
    const was = this.status;
    this.status = status;
    if (was !== status) this.emit("connection", { status });
  }

  /* ---------------------------------------------------------------- live state --- */
  async poll() {
    clearTimeout(this.pollT);
    if (!this.connected) return;
    try {
      if (this.perform) this.setLive(parsePerform(await this.link.request(CMD.PERFORM, [PF.STATE], { key: "state" })));
      else {
        await this.link.request(CMD.PING, [], { key: "ping" });
        this.pollN = (this.pollN || 0) + 1;
        if (this.pollN % 8 === 0) await this.readTracks();
      }
      this.stale = false;
    } catch (e) {
      if (!this.connected) return;
      this.stale = true;
    }
    this.emit("health", { stale: !!this.stale });
    this.pollT = setTimeout(() => this.poll(), POLL_MS);
  }
  setLive(s) {
    const prevSel = this.sel;
    this.live = s;
    for (let i = 0; i < this.tracks.length; i++) this.tracks[i].mute = !!(s.mute >> i & 1);
    this.solo = s.solo;
    if (this.g.BPM) this.g.BPM.value = s.bpm;
    /* the device picked another track (a reply sent before our own TRACK select is stale: ignored a moment) */
    if (s.sel >= 0 && s.sel !== prevSel && performance.now() - (this.selAt || 0) > 600) { this.sel = s.sel; this.loadSound(); this.emit("tracks"); }
    this.emit("state", s);
  }
  /* a PERFORM op (a pad, a button): ahead of the polls; its reply is the state */
  async op(op, args = [], key) {
    if (!this.connected || !this.perform) return null;
    try {
      const s = parsePerform(await this.link.request(CMD.PERFORM, [op, ...args], { front: true, key, timeout: 250 }));
      this.setLive(s);
      return s;
    } catch (e) { return null; }
  }

  /* ---------------------------------------------------------------- tracks, mix --- */
  async readTracks() {
    const t = parseTrack(await this.link.request(CMD.TRACK, []));
    const old = this.tracks || [];
    this.tracks = t.tracks.map((x, i) => Object.assign(old[i] || {}, x));
    this.solo = t.solo;
    const selChanged = this.sel !== t.sel;
    this.sel = t.sel;
    this.emit("tracks");
    if (this.connected && (selChanged || !this.sound)) this.loadSound();
    if (this.connected) this.prefetchNames();
  }
  /* the preset names of the tracks' engines (the tiles call a track by its preset) */
  async prefetchNames() {
    for (const t of this.tracks || []) {
      if (!this.connected) return;
      if (t.eng < this.drumEng && !this.names[t.eng]) {
        try { this.names[t.eng] = parseNames(await this.link.request(CMD.NAMES, [t.eng])); } catch (_) { return; }
      }
    }
    this.emit("tracks");
  }
  setLevel(t, level) {
    const tr = this.tracks[t];
    tr.level = Math.round(level);
    this.link.request(CMD.TRACK_MIX, [t, ...v14(tr.level), tr.mute ? 1 : 0], { key: "mix" + t }).catch(() => {});
  }
  setGlobal(label, value) {
    const p = this.g[label];
    if (!p || !this.link) return;
    p.value = Math.max(p.desc.min, Math.min(p.desc.max, Math.round(value)));
    this.link.request(CMD.SET, [1, p.id, ...v14(p.value)], { key: "g" + p.id }).catch(() => {});
    this.emit("globals", { label });
  }

  /* ---------------------------------------------------------------- the selected track's sound --- */
  async selectTrack(t) {
    if (!this.connected || t === this.sel) return;
    this.sel = t;
    this.selAt = performance.now();
    this.emit("tracks");
    await this.link.request(CMD.TRACK, [t]).catch(() => {});
    await this.loadSound();
  }
  /* the selected track's values, and the descriptors its sound needs (read once, then cached) */
  async loadSound() {
    if (!this.connected) return;
    const t = this.sel, my = (this.soundSeq = (this.soundSeq || 0) + 1);
    try {
      const d = parseDump(await this.link.request(CMD.TRACK_DUMP, [t]), this.info.pCount);
      if (my !== this.soundSeq) return;
      const tr = this.tracks[t];
      tr.eng = d.eng; tr.preset = d.preset;
      const drum = d.eng >= this.drumEng;
      if (!Object.keys(this.tdesc).length) {
        for (const [k, id] of Object.entries(TP_IDS)) {
          if (id >= this.info.pE0) continue;
          const desc = parseDesc(await this.link.request(CMD.DESC, [0, id]));
          if (desc.label === TP_LABEL[k]) this.tdesc[id] = desc;
        }
      }
      /* the engine's eight, read each time: some engines name them by their mode (SAMPLE, NOISE...) */
      const ek = drum ? "drum" : d.eng, ds = [];
      for (let k = 0; k < 8; k++) ds.push(parseDesc(await this.link.request(CMD.DESC, [0, this.info.pE0 + k])));
      this.engDesc[ek] = ds;
      if (!drum && !this.names[d.eng]) this.names[d.eng] = parseNames(await this.link.request(CMD.NAMES, [d.eng]));
      if (my !== this.soundSeq) return;
      this.sound = { track: t, eng: d.eng, preset: d.preset, drum, p: d.p, edit: this.engDesc[ek] };
      this.emit("sound");
    } catch (e) { /* (unplugged meanwhile) */ }
  }
  desc(id) { return id >= this.info.pE0 ? this.sound && this.sound.edit[id - this.info.pE0] : this.tdesc[id]; }
  setParam(id, value) {
    const s = this.sound, d = this.desc(id);
    if (!s || !d) return;
    s.p[id] = Math.max(d.min, Math.min(d.max, Math.round(value)));
    this.link.request(CMD.TRACK_PARAM, [s.track, id, ...v14(s.p[id])], { key: "p" + s.track + ":" + id }).catch(() => {});
  }
  async preset(eng, preset) {
    const r = new Reader(await this.link.request(CMD.PRESET, [eng, preset]));
    r.b(); r.b();
    await this.loadSound();
  }
  async engineNames(eng) {
    if (!this.names[eng]) this.names[eng] = parseNames(await this.link.request(CMD.NAMES, [eng]));
    return this.names[eng];
  }
  async userPresets() {
    const a = parseUpList(await this.link.request(CMD.UP_LIST, [0, 16]));
    const b = a.total > 16 ? parseUpList(await this.link.request(CMD.UP_LIST, [16, 16])) : { slots: [] };
    return a.slots.concat(b.slots);
  }
  async loadUser(slot) {
    const rc = new Reader(await this.link.request(CMD.UP_LOAD, [slot])).a[1];
    await this.loadSound();
    return rc;
  }
  async storeUser(slot, name) {
    const a = await this.link.request(CMD.UP_STORE, [slot, ...[...name].map((c) => c.charCodeAt(0) & 127), 0], { timeout: 1500 });
    return a[1];
  }

  /* ---------------------------------------------------------------- pushes --- */
  onPush(f) {
    const r = new Reader(f.a);
    if (f.cmd === CMD.CHANGED) {
      const scope = r.b(), id = r.b(), v = r.v();
      if (scope === 1) {
        for (const [label, p] of Object.entries(this.g)) if (p.id === id) { p.value = v; this.emit("globals", { label }); }
      } else if (this.sound && this.sound.track === this.sel) {
        this.sound.p[id] = v;
        this.emit("sound", { id });
      }
    } else if (f.cmd === CMD.RELOAD) {
      this.readTracks().then(() => this.loadSound()).catch(() => {});
    } else if (f.cmd === CMD.TRACK_CHANGED) {
      this.readTracks().catch(() => {});
    }
  }
}

/* USB: keeps the browser's MIDI access, connects when an FM-1 shows up (and at start when the permission was
   given before), and lets go when it goes away */
export class AutoMidi {
  constructor(onAppear) { this.onAppear = onAppear; this.access = null; }
  static get supported() { return !!navigator.requestMIDIAccess; }
  /* "granted" | "prompt" | "denied" | "unknown" */
  static async permission() {
    try { return (await navigator.permissions.query({ name: "midi", sysex: true })).state; } catch (_) { return "unknown"; }
  }
  async start() {
    if (this.access) return this.access;
    this.access = await navigator.requestMIDIAccess({ sysex: true });
    this.access.addEventListener("statechange", (e) => {
      if (e.port && /felucca|sloop/i.test(e.port.name || "") && e.port.state === "connected") {
        clearTimeout(this.t);
        this.t = setTimeout(() => this.onAppear(), 400);   /* (both ports, and the FM-1 done booting) */
      }
    });
    return this.access;
  }
  present() {
    if (!this.access) return false;
    const has = (m) => [...m.values()].some((p) => /felucca|sloop/i.test(p.name || "") && p.state === "connected");
    return has(this.access.inputs) && has(this.access.outputs);
  }
}
