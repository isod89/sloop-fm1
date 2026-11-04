// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
// SLOOP in an AudioWorklet: sloop.wasm is the whole FM-1 firmware against a simulated FM-1
// (web/emu/sloop_web.c). Each render quantum asks it for 128 frames, which runs the device's clock forward;
// between quanta the page's input goes in and, about 30 times a second (4 when the page is hidden), the screen,
// the lights and any changed flash go out. After X0X's web/emu/worklet.js (charlesvestal/fm1-x0x, GPL-3.0).

// The flash the page keeps: storage.c's areas (projects, user presets and their FM6 patches, the user sample
// slots 0x97000..0xDFFFF; the autosave 0xE5000..0xE6FFF (1.2) and FM6's voice bank 0xE7000 (1.4.1); the settings and
// the second FM6 copy 0xFC000..0xFFFFF), only the sectors that hold something (an erased sector is all 0xFF). {offset: bytes}
// SLOOP's layout differs from Felucca's (samples USR1-3 at 0xA0000, USR4 at 0xE7000, the FM6 bank at
// 0xE5000, the OTA area at 0xE0000), so the whole 1 MiB is scanned: an erased sector is all 0xFF and
// is skipped, and only what the firmware actually wrote goes to the page.
export const STORE_AREAS = [[0x00000, 0x100000]];
export function storeSectors(nor) {
  const out = {};
  for (const [a, b] of STORE_AREAS)
    for (let off = a; off < b; off += 4096) {
      const s = nor.subarray(off, off + 4096);
      if (s.some((v) => v !== 0xFF)) out[off] = s.slice();
    }
  return out;
}
export function restoreSectors(nor, sectors) {
  for (const [k, v] of Object.entries(sectors || {})) {
    const off = +k, b = v instanceof Uint8Array ? v : new Uint8Array(v);
    if (STORE_AREAS.some(([a, e]) => off >= a && off + 4096 <= e && off % 4096 === 0) && b.length === 4096)
      nor.set(b, off);
  }
}

export const HOLD_MS = 24;

// The panel's knobs from a MIDI controller (the page's Web MIDI in): CC 20..26 turn SELECT, ALGORITHM, PRESETS,
// KNOB 1..4 (the encoder roles), a detent a step; CC 27 sets MASTER (a pot on the FM-1: absolute in every mode). The
// firmware's CC map (midi_control.c) has none of them, so they stay here. mode, as the controller sends a turn:
//   abs  absolute (the default): by how far the value moves, so it takes over from anywhere without a jump; last:
//        the value each knob CC sent before (the first one only takes its place)
//   bin  relative, 64 +- n (65 = +1, 63 = -1)
//   twos relative, two's complement (1 = +1, 127 = -1)
//   sign relative, sign bit (1 = +1, 65 = -1)
// -> {role, n} | {master: 0..1023} | null (not one of them: on to the firmware)
export const KNOB_CC = 20, MASTER_CC = 27, KNOB_MODES = ["abs", "bin", "twos", "sign"];
export function midiKnob(last, cc, value, mode = "abs") {
  if (cc === MASTER_CC) return { master: Math.round(value * 1023 / 127) };
  const role = cc - KNOB_CC;
  if (role < 0 || role >= 7) return null;
  if (mode === "bin") return { role, n: value - 64 };
  if (mode === "twos") return { role, n: value < 64 ? value : value - 128 };
  if (mode === "sign") return { role, n: value < 64 ? value : 64 - value };
  const was = last[role];
  last[role] = value;
  return { role, n: was === undefined ? 0 : value - was };
}

if (typeof registerProcessor === "function") {
  const clock = globalThis.performance ? () => globalThis.performance.now() : () => Date.now();
  class Felucca extends AudioWorkletProcessor {
    constructor() {
      super();
      this.ex = null;
      this.sentDraws = -1;
      this.sentWrites = -1;
      this.sentLeds = "";
      this.lastFrame = 0;
      this.period = 1 / 30;
      this.busy = 0;
      this.frames = 0;
      this.queue = [];                                // button / key changes, each held >= HOLD_MS of device time
      this.nextOk = 0;
      this.port.onmessage = (e) => this.onMessage(e.data);
    }

    async onMessage(m) {
      if (m.type === "load") {
        try {
          const { instance } = await WebAssembly.instantiate(m.wasm, {});
          const ex = instance.exports;
          if (ex._initialize) ex._initialize();   // (Emscripten's STANDALONE_WASM; the clang build has none)
          ex.web_nor_erase();
          restoreSectors(new Uint8Array(ex.memory.buffer, ex.web_nor(), ex.web_nor_size()), m.sectors);
          ex.web_master(m.master ?? 700);
          ex.web_boot();
          this.ex = ex;
          this.sentWrites = ex.web_flash_writes_count();
          this.port.postMessage({ type: "ready", rate: ex.web_sample_rate() });
        } catch (err) {
          this.port.postMessage({ type: "error", message: String(err && err.message || err) });
        }
        return;
      }
      const ex = this.ex;
      if (!ex) return;
      if (m.type === "buttons" || m.type === "keys") this.queue.push(m);
      else if (m.type === "enc") ex.web_enc(m.role, m.n | 0);
      else if (m.type === "master") ex.web_master(m.value | 0);
      else if (m.type === "midi") ex.web_midi(m.data[0] | 0, m.data[1] | 0, m.data[2] | 0);
      else if (m.type === "visible") this.period = m.on ? 1 / 30 : 1 / 4;
      else if (m.type === "objects") this.loadObjects(m);
    }

    // a .sloopsong or a backup: its objects are the firmware's own structs, written into the flash as the
    // editor's BK_PUT would write them (sloop_web.c web_obj_put / web_order_put)
    loadObjects(m) {
      const ex = this.ex, bad = [], cap = ex.web_upload_size();
      const put = (bytes) => {
        new Uint8Array(ex.memory.buffer, ex.web_upload_buf(), bytes.length).set(bytes);
      };
      for (const o of m.objects) {
        const b = o.data instanceof Uint8Array ? o.data : new Uint8Array(o.data);
        if (!b.length || b.length > cap) { bad.push(o.id); continue; }
        put(b);
        if (ex.web_obj_put(o.id, b.length)) bad.push(o.id);
      }
      if (m.order) {
        const o = m.order instanceof Uint8Array ? m.order : new Uint8Array(m.order);
        put(o);
        if (ex.web_order_put(o.length)) bad.push("order");
      }
      this.publish();                       // hand the written sectors to the page before it reloads
      this.port.postMessage({ type: "loaded", bad });
    }

    publish() {
      const ex = this.ex, mem = ex.memory.buffer;
      const msg = { type: "frame" };
      // every one optional: a missing export must never take the audio processor down with it
      const call = (n) => (typeof ex[n] === "function" ? ex[n]() : 0);
      const leds = [call("web_lit_buttons"), call("web_lit_keys"), call("web_dim_buttons"), call("web_dim_keys"),
                    call("web_dim_level"), call("web_breath_buttons"), call("web_breath_keys"), call("web_mid_keys")];
      const an = ex.web_anim_levels ? ex.web_anim_levels() : 0;   // the power-on sweep: each LED's level
      const anim = an ? Array.from(new Uint8Array(mem, an, 41)) : null;
      const key = leds.join(",") + (anim ? ";" + anim.join(",") : "");
      if (key !== this.sentLeds) {
        this.sentLeds = key;
        msg.leds = leds;
        msg.anim = anim;
      }
      if (this.frames >= 44100) {                     // the share of real time spent running the device
        msg.load = this.busy / (this.frames / 44.1);
        this.busy = this.frames = 0;
      }
      const transfer = [];
      const draws = ex.web_screen_draws();
      if (draws !== this.sentDraws) {
        this.sentDraws = draws;
        msg.fb = new Uint16Array(mem, ex.web_screen(), 240 * 240).slice();
        transfer.push(msg.fb.buffer);
      }
      const writes = ex.web_flash_writes_count();
      if (writes !== this.sentWrites) {               // a save is done within one main-loop pass: whole here
        this.sentWrites = writes;
        msg.sectors = storeSectors(new Uint8Array(mem, ex.web_nor(), ex.web_nor_size()));
      }
      if (msg.leds || msg.fb || msg.sectors || msg.load !== undefined) this.port.postMessage(msg, transfer);
    }

    // A click's down and up can arrive within one render quantum, but the FM-1's contacts never close for less than
    // its debounce, and the UI reads them once a frame: each change of the buttons or the keys stays HOLD_MS of the
    // device's time before the next one goes in (a tap: down for a frame and more, then up).
    input() {
      const ex = this.ex, now = ex.web_now_ms();
      while (this.queue.length && now - this.nextOk >= 0) {
        const m = this.queue.shift();
        if (m.type === "buttons") ex.web_buttons(m.mask >>> 0);
        else ex.web_keys(m.mask >>> 0);
        this.nextOk = now + HOLD_MS;
      }
    }

    process(inputs, outputs) {
      const ex = this.ex;
      if (!ex) return true;
      if (this.queue.length) this.input();
      const out = outputs[0], n = out[0].length;
      const t0 = clock();
      ex.web_render(n);
      this.busy += clock() - t0;
      this.frames += n;
      out[0].set(new Float32Array(ex.memory.buffer, ex.web_out_l(), n));
      if (out[1]) out[1].set(new Float32Array(ex.memory.buffer, ex.web_out_r(), n));
      if (currentTime - this.lastFrame >= this.period) {
        this.lastFrame = currentTime;
        this.publish();
      }
      return true;
    }
  }
  registerProcessor("sloop", Felucca);
}
