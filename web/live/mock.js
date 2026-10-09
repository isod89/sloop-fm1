// SPDX-License-Identifier: GPL-3.0-only
// A pretend FM-1 running SLOOP (protocol v11), for trying SLOOP live without the synth: ?mock=1 (?mock=10: a
// firmware without PERFORM). It answers from the firmware's own tables (mockdata.js), runs a clock, plays
// sections, chains and fills, keeps four tracks' sounds, and now and then turns a knob "on the device" so
// the live sync shows.
import { CMD, PF, NONE, frame, unframe, v14 } from "./proto.js";
import { MOCK } from "./mockdata.js";

const str = (s) => [...s].map((c) => c.charCodeAt(0) & 127).concat(0);
const D = (d) => ({ label: d[0], fmt: d[1], min: d[2], max: d[3], def: d[4], unit: d[5], names: d[6] });
const clamp = (v, d) => Math.max(d.min, Math.min(d.max, v));
const descBytes = (scope, id, d) => [scope, id, d.fmt, ...v14(d.min), ...v14(d.max), ...v14(d.def), ...str(d.label), ...str(d.unit || ""),
  ...(d.fmt === 8 && d.names ? d.names.flatMap(str) : [])];

export class MockTransport {
  constructor(proto = 11) { this.proto = proto; }
  get label() { return "demo"; }
  async open(onData) {
    const NE = MOCK.engines.length, DRUM = NE, P0 = MOCK.pE0, PC = MOCK.pCount;
    const TD = MOCK.track.map(D), GD = MOCK.global.map(D), KIT = D(MOCK.kit);
    const g = GD.map((d) => d.def);
    const gi = (l) => GD.findIndex((d) => d.label === l);
    g[gi("BPM")] = 96; g[gi("SWING")] = 24; g[gi("DUCK")] = 30;
    const engDesc = (e, k) => D(MOCK.engines[e].edit[k]);
    /* four tracks: a bass, keys, a pad, the drums */
    const mk = (eng, preset, level) => {
      const p = TD.map((d) => d.def);
      for (let k = 0; k < 8; k++) p[P0 + k] = eng === DRUM ? (k === 0 ? 9 : 0) : engDesc(eng, k).def;
      p[0] = level;
      return { eng, preset, p };
    };
    const trk = [mk(0, 2, 100), mk(1, 3, 88), mk(6, 1, 72), mk(DRUM, 0, 100)];
    trk[2].p[40] = 1;                                   /* track 3 starts muted */
    let sel = 0;
    const st = { playing: true, beat: 0, fx: NONE, fxRemote: 0, solo: 0, ready: 0b0011, bars: [1, 2, 0, 0], section: 0, next: NONE,
      songMode: 0, songrec: 0, chain: [], chainI: 0, barIn: 0, fillHeld: 0, fillNext: 0, taps: [], storeArm: -1, storeT: 0 };
    const up = Array.from({ length: 32 }, () => null);
    up[0] = { eng: 0, name: "MY WOBBLE", p: mk(0, 0, 100).p };
    up[1] = { eng: 6, name: "DUSTY KEYS", p: mk(6, 2, 90).p };
    let last = performance.now(), watch = 0;
    const send = (cmd, a) => setTimeout(() => onData(frame(cmd, a)), 4 + Math.random() * 10);
    const reply = send;
    const secBars = (s) => (st.ready >> s & 1 ? st.bars[s] : 0);
    const tick = () => {
      if (!st.playing) return;
      st.beat = (st.beat + 1) & 127;
      if (st.beat % 4) return;
      st.barIn++;
      st.fillNext = 0;
      const len = secBars(st.section) || 1;
      if (st.chain.length && st.next === NONE && st.barIn >= len) {
        st.chainI = (st.chainI + 1) % st.chain.length;
        st.next = st.chain[st.chainI];
      }
      if (st.next !== NONE) { st.section = st.next; st.next = NONE; st.barIn = 0; st.beat = 0; }
      else if (st.barIn >= len) { st.barIn = 0; st.beat = 0; }
    };
    const loop = () => { tick(); this.timer = setTimeout(loop, 60000 / g[gi("BPM")]); };
    loop();
    /* "the device": now and then a knob of the selected track moves (the live sync) */
    this.knob = setInterval(() => {
      if (!watch || Math.random() < 0.5) return;
      const id = P0 + 4, t = trk[sel];
      if (t.eng === DRUM) return;
      const d = engDesc(t.eng, 4);
      t.p[id] = clamp(t.p[id] + (Math.random() < 0.5 ? -6 : 6), d);
      send(CMD.CHANGED, [0, id, ...v14(t.p[id])]);
    }, 2500);
    const state = () => {
      const mute = trk.reduce((m, t, i) => m | (t.p[40] ? 1 << i : 0), 0);
      const flags = (st.playing ? 1 : 0) | (st.songMode ? 2 : 0) | (st.fillHeld ? 8 : 0) | (st.fillNext ? 16 : 0) | (st.fxRemote ? 32 : 0);
      return [flags, ...v14(g[gi("BPM")]), st.beat, st.fx, mute, st.solo, 0, st.ready, st.section, st.next, st.songrec,
        st.chain.length, st.chainI, ...st.chain, sel, secBars(0), secBars(1), secBars(2), secBars(3)];
    };
    const perform = (a) => {
      const op = a.length ? a[0] : 0, x = a.slice(1);
      let rc = 0;
      switch (op) {
        case PF.STATE: break;
        case PF.FX: st.fx = x[0] < 16 ? x[0] : NONE; st.fxRemote = st.fx !== NONE ? 1 : 0; break;
        case PF.SECTION:
          if (x[0] > 3) { rc = 1; break; }
          if (!(st.ready >> x[0] & 1)) break;
          st.chain = [];
          if (st.playing) st.next = x[0]; else { st.section = x[0]; st.beat = 0; st.barIn = 0; }
          break;
        case PF.CHAIN: {
          const s = x.slice(1, 1 + x[0]);
          if (!st.playing) { rc = 1; break; }
          if (s.some((k) => !(st.ready >> k & 1))) { rc = 2; break; }
          st.next = s[0]; st.chain = s.length > 1 ? s : []; st.chainI = 0;
          break;
        }
        case PF.MUTE: trk[x[0]].p[40] = x[1] === 2 ? (trk[x[0]].p[40] ? 0 : 1) : x[1]; break;
        case PF.SOLO: { const on = x[1] === 2 ? !(st.solo >> x[0] & 1) : x[1]; st.solo = on ? st.solo | 1 << x[0] : st.solo & ~(1 << x[0]); break; }
        case PF.TRANSPORT:
          st.playing = x[0] === 0 ? !st.playing : x[0] === 1;
          if (st.playing) { st.beat = 127; st.barIn = -1; } else { st.fillHeld = st.fillNext = 0; }
          break;
        case PF.FILL: if (x[0] === 2) st.fillNext = st.fillNext ? 0 : 1; else st.fillHeld = x[0]; break;
        case PF.TAP: {
          const now = performance.now();
          st.taps = st.taps.filter((t) => now - t < 2000).concat(now);
          if (st.taps.length >= 2) g[gi("BPM")] = Math.max(40, Math.min(240, Math.round(60000 / ((st.taps.at(-1) - st.taps[0]) / (st.taps.length - 1)))));
          break;
        }
        case PF.SONGMODE: st.songMode = x[0] === 2 ? (st.songMode ? 0 : 1) : x[0]; break;
        case PF.SONGREC: st.songrec = st.songrec ? 0 : 1; break;
        case PF.STORE: {
          const now = performance.now();
          if ((st.ready >> x[0] & 1) && !(st.storeArm === x[0] && now - st.storeT < 3000)) { st.storeArm = x[0]; st.storeT = now; break; }
          st.storeArm = -1; st.ready |= 1 << x[0]; st.section = x[0]; st.bars[x[0]] = 1 + (Math.random() * 2 | 0) * 1;
          break;
        }
        default: rc = 1;
      }
      return [op, rc, ...state()];
    };
    const tdesc = (id) => {
      const t = trk[sel];
      if (id < P0) return TD[id];
      if (t.eng === DRUM) return id === P0 ? KIT : engDesc(0, id - P0);
      return engDesc(t.eng, id - P0);
    };
    const applyPreset = (t, eng, preset) => {
      const fresh = mk(eng, preset, t.p[0]);
      for (const k of [0, 39, 40, 29, 30, 31, 32, 25, 26, 27, 49]) fresh.p[k] = t.p[k];   /* the mix, the pattern, the key stay */
      /* a different preset: a few sound values move, so revert shows */
      if (eng !== DRUM) for (let k = 0; k < 8; k++) { const d = engDesc(eng, k); if (d.fmt !== 8) fresh.p[P0 + k] = clamp(d.def + ((preset * 13 + k * 7) % 41) - 20, d); }
      Object.assign(t, fresh);
    };
    this.dog = setInterval(() => {
      if (performance.now() - last > 1500) { if (st.fxRemote) { st.fx = NONE; st.fxRemote = 0; } st.fillHeld = 0; }
      if (performance.now() - last > 3000) watch = 0;
    }, 250);
    this.send = (d) => {
      const f = unframe(d);
      if (!f) return;
      const a = f.a;
      last = performance.now();
      switch (f.cmd) {
        case CMD.INFO:
          reply(f.cmd, [...str(MOCK.version), NE, PC, GD.length, MOCK.nStep, P0, ...MOCK.engines.flatMap((e) => str(e.name)), 4, this.proto]);
          break;
        case CMD.DESC: {
          const d = a[0] === 1 ? GD[a[1]] : a[1] < PC ? tdesc(a[1]) : null;
          if (d) reply(f.cmd, descBytes(a[0], a[1], d));
          break;
        }
        case CMD.GET: reply(f.cmd, [a[0], a[1], ...v14(a[0] === 1 ? g[a[1]] : trk[sel].p[a[1]])]); break;
        case CMD.SET:
          if (a[0] === 1) g[a[1]] = clamp((a[2] | a[3] << 7) - 8192, GD[a[1]]);
          else trk[sel].p[a[1]] = clamp((a[2] | a[3] << 7) - 8192, tdesc(a[1]));
          reply(f.cmd, [a[0], a[1], ...v14(a[0] === 1 ? g[a[1]] : trk[sel].p[a[1]])]);
          break;
        case CMD.WATCH: watch = a[0] & 3; reply(f.cmd, [a[0] & 3]); break;
        case CMD.PING: reply(f.cmd, [0]); break;
        case CMD.TRACK:
          if (a.length && a[0] < 4) sel = a[0];
          reply(f.cmd, [sel, 4, ...trk.flatMap((t, i) => [t.eng, t.preset, ...v14(i === 3 ? g[gi("LVL")] : t.p[0]), t.p[40] ? 1 : 0, 0]), st.solo]);
          break;
        case CMD.TRACK_MIX: {
          const t = trk[a[0]];
          if (a.length >= 4) { const v = Math.max(0, Math.min(127, (a[1] | a[2] << 7) - 8192)); if (a[0] === 3) g[gi("LVL")] = v; else t.p[0] = v; t.p[40] = a[3] ? 1 : 0; }
          reply(f.cmd, [a[0], ...v14(a[0] === 3 ? g[gi("LVL")] : t.p[0]), t.p[40] ? 1 : 0]);
          break;
        }
        case CMD.TRACK_DUMP: { const t = trk[a[0]]; reply(f.cmd, [a[0], t.eng, t.preset, ...t.p.flatMap((v) => v14(v))]); break; }
        case CMD.TRACK_PARAM: {
          const t = trk[a[0]];
          if (a.length >= 4) { const o = sel; sel = a[0]; t.p[a[1]] = clamp((a[2] | a[3] << 7) - 8192, tdesc(a[1])); sel = o; }
          reply(f.cmd, [a[0], a[1], ...v14(t.p[a[1]])]);
          break;
        }
        case CMD.NAMES: { const e = MOCK.engines[a[0]]; if (e) reply(f.cmd, [a[0], e.presets.length, ...e.presets.flatMap(str), ...e.pages.flatMap(str)]); break; }
        case CMD.PRESET: {
          const t = trk[sel];
          if (t.eng !== DRUM && a[0] < NE) applyPreset(t, a[0], Math.min(a[1], MOCK.engines[a[0]].presets.length - 1));
          reply(f.cmd, [t.eng, t.preset]);
          break;
        }
        case CMD.UP_LIST: {
          const s0 = a[0], n = Math.min(a[1], 32 - s0);
          reply(f.cmd, [s0, Math.max(0, n), 32, ...Array.from({ length: Math.max(0, n) }, (_, i) => up[s0 + i]).flatMap((u) => [u ? 1 : 0, u ? u.eng : 0, ...str(u ? u.name : "")])]);
          break;
        }
        case CMD.UP_LOAD: {
          const u = up[a[0]], t = trk[sel];
          if (u && t.eng !== DRUM) { const keep = t.p.slice(); t.eng = u.eng; t.preset = 0; t.p = u.p.slice(); for (const k of [0, 39, 40, 29, 30, 31, 32]) t.p[k] = keep[k]; }
          reply(f.cmd, [a[0], u && t.eng !== DRUM ? 0 : 1]);
          break;
        }
        case CMD.UP_STORE: {
          const t = trk[sel], name = String.fromCharCode(...a.slice(1, a.indexOf(0, 1) < 0 ? undefined : a.indexOf(0, 1)));
          if (t.eng === DRUM || a[0] > 31) { reply(f.cmd, [a[0], 1]); break; }
          up[a[0]] = { eng: t.eng, name: (name || MOCK.engines[t.eng].name + " " + (a[0] + 1)).toUpperCase().slice(0, 12), p: t.p.slice() };
          reply(f.cmd, [a[0], 0]);
          break;
        }
        case CMD.PERFORM: if (this.proto >= 11) reply(f.cmd, perform(a)); break;
        default: break;
      }
    };
    this.close = () => { clearTimeout(this.timer); clearInterval(this.dog); clearInterval(this.knob); };
  }
}
