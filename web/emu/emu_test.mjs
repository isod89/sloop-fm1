// SPDX-License-Identifier: GPL-3.0-only
// The browser build (build/emu/sloop.wasm, web/emu/sloop_web.c) in Node, as the page's AudioWorklet runs it.
//   node web/emu/emu_test.mjs build/emu/sloop.wasm
// After Felucca's web/emu/emu_test.mjs (Leo Kuroshita / Hügelton Instruments, GPL-3.0), itself after X0X's
// (charlesvestal/fm1-x0x, GPL-3.0).
//
// Checks: the module has no imports and every export the page calls; it boots (the splash, then HOME on the
// screen, the HOME LED); a stopped empty project is silent and a key sounds, bounded and finite, on two
// separate channels; the device clock follows the audio; the two numberings the page and the firmware use
// (labels vs matrix ids, key numbers vs key ids) meet; a button redraws the screen; the encoders turn; the
// autosave reaches the emulated flash and the sectors the page keeps bring it back in a fresh instance; a
// song file's objects and order can be written as the editor writes them; and 1 s of audio costs far less
// than 1 s of real time.
import fs from "node:fs";
import { storeSectors, restoreSectors } from "./worklet.js";

const wasmPath = process.argv[2] || "build/emu/sloop.wasm";
const bytes = fs.readFileSync(wasmPath);

// panel.c's labels (the page's bit numbers) and its encoder roles
const B = { FX: 0, SEL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, OCTDN: 12, OCTUP: 13 };
const EN = { SELECT: 0, ALGORITHM: 1, PRESETS: 2, K1: 3, K2: 4, K3: 5, K4: 6 };
const MATRIX = [2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 1];   // panel.c PANEL_DEFAULT.btn[]
const LABELS = Object.keys(B);
const FS = 44100;

let fails = 0;
const check = (what, ok, detail = "") => {
  console.log(`emu: ${what.padEnd(74)} ${ok ? "ok" : "FAIL"}${detail ? "  (" + detail + ")" : ""}`);
  if (!ok) fails++;
};

const NEEDED = ["web_nor", "web_nor_size", "web_nor_erase", "web_flash_writes_count", "web_master",
  "web_boot", "web_sample_rate", "web_now_ms", "web_buttons", "web_keys", "web_enc", "web_midi",
  "web_screen", "web_screen_draws", "web_lit_buttons", "web_lit_keys", "web_dim_buttons", "web_dim_keys",
  "web_dim_level", "web_breath_buttons", "web_breath_keys", "web_mid_keys", "web_out_l", "web_out_r",
  "web_render", "web_debug_buttons", "web_debug_notes", "web_upload_buf", "web_upload_size",
  "web_obj_put", "web_order_put"];

async function device(saved) {
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports, mem = ex.memory;
  ex.web_nor_erase();
  if (saved) restoreSectors(new Uint8Array(mem.buffer, ex.web_nor(), ex.web_nor_size()), saved);
  ex.web_master(700);
  ex.web_boot();
  const d = { ex, mem, peak: 0, left: 0, right: 0 };
  d.render = (ms) => {
    const n = Math.round((ms * FS) / 1000);
    for (let k = 0; k < n; k += 128) {
      const m = Math.min(128, n - k);
      ex.web_render(m);
      const l = new Float32Array(mem.buffer, ex.web_out_l(), m);
      const r = new Float32Array(mem.buffer, ex.web_out_r(), m);
      for (let i = 0; i < m; i++) {
        d.peak = Math.max(d.peak, Math.abs(l[i]), Math.abs(r[i]));
        d.left += Math.abs(l[i]);
        d.right += Math.abs(r[i]);
      }
    }
  };
  d.tap = (label) => { ex.web_buttons(1 << label); d.render(60); ex.web_buttons(0); d.render(60); };
  d.turn = (role, n) => { for (let i = 0; i < Math.abs(n); i++) { ex.web_enc(role, Math.sign(n)); d.render(40); } };
  d.screen = () => new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240).slice();
  d.sectors = () => storeSectors(new Uint8Array(mem.buffer, ex.web_nor(), ex.web_nor_size()));
  return d;
}

const colours = (fb) => new Set(fb).size;
const lit = (fb) => fb.reduce((n, px) => n + (px !== 0 ? 1 : 0), 0);
const same = (x, y) => x.length === y.length && x.every((v, i) => v === y[i]);

// ---- the module
const mod = await WebAssembly.compile(bytes);
check("no imports (nothing of the browser's, no host libc)", WebAssembly.Module.imports(mod).length === 0);
{
  const instance = await WebAssembly.instantiate(mod, {});   // a Module gives the Instance itself
  const missing = NEEDED.filter((n) => typeof instance.exports[n] !== "function");
  check(`every export the page calls is there`, missing.length === 0, missing.join(", ") || `${NEEDED.length}/${NEEDED.length}`);
}

// ---- boot
const a = await device(null);
a.render(300);
const splash = a.screen();
a.render(900);
const home = a.screen();
check("the splash is drawn", colours(splash) > 1, `${colours(splash)} colours`);
check("then HOME: a real screen", colours(home) > 4 && lit(home) > 5000, `${colours(home)} colours, ${lit(home)} px lit`);
check("the HOME button's LED is lit at boot", (a.ex.web_lit_buttons() >> B.HOME) & 1, `lit=0x${a.ex.web_lit_buttons().toString(16)}`);
check("the device clock runs with the audio", Math.abs(a.ex.web_now_ms() - 1200) < 60, `${a.ex.web_now_ms()} ms after 1.2 s`);
check("the sample rate is the firmware's", a.ex.web_sample_rate() === FS, `${a.ex.web_sample_rate()} Hz`);

// ---- silence, a key, two channels
a.peak = 0;
a.render(400);
check("a stopped empty project is silent", a.peak === 0, `peak ${a.peak}`);

a.peak = 0; a.left = 0; a.right = 0;
a.ex.web_keys(1);                                   // F3: track 1, the default 808 BOOM
a.render(400);
a.ex.web_keys(0);
a.render(300);
check("a key sounds", a.peak > 0.01, `peak ${a.peak.toFixed(3)}`);
check("the audio stays inside full scale (the limiter holds)", a.peak <= 1.0, `peak ${a.peak.toFixed(3)}`);
check("both channels carry the sound", a.left > 0 && a.right > 0, `L ${a.left.toFixed(1)} / R ${a.right.toFixed(1)}`);
check("the two channels are separate buffers, not one interleaved",
      a.ex.web_out_l() !== a.ex.web_out_r(), `L@${a.ex.web_out_l()} R@${a.ex.web_out_r()}`);

// ---- the page's numbering vs the firmware's (on its own device: pressing PLAY and REC here would leave the
// transport running, and the autosave below wants it stopped)
const m = await device(null);
m.render(1200);
const badBtn = [];
for (let i = 0; i < 14; i++) {
  m.ex.web_buttons(1 << i);
  m.render(40);
  if (m.ex.web_debug_buttons() !== (1 << MATRIX[i])) badBtn.push(LABELS[i]);
  m.ex.web_buttons(0);
  m.render(40);
}
check("every button reaches its matrix id (panel.btn[])", badBtn.length === 0, badBtn.join(", ") || "14/14");

const badKey = [];
for (const k of [0, 13, 26]) {
  m.ex.web_keys(1 << k);
  m.render(40);
  if (m.ex.web_debug_notes() !== (1 << k)) badKey.push(k);
  m.ex.web_keys(0);
  m.render(40);
}
check("keys reach the firmware unshifted (bit k = key k)", badKey.length === 0, badKey.join(", ") || "keys 0, 13, 26");

// ---- input redraws, the encoders turn
{
  const d0 = a.ex.web_screen_draws();
  a.tap(B.GLO);
  check("a button press redraws the screen", a.ex.web_screen_draws() > d0, `+${a.ex.web_screen_draws() - d0}`);
  a.tap(B.HOME);
}
{
  a.ex.web_buttons(1 << B.HOME); a.render(80);      // (back on TRACKS)
  const before = a.screen();
  a.turn(EN.ALGORITHM, 1);                          // the next track: the whole editor changes colour
  const after = a.screen();
  a.ex.web_buttons(0); a.render(80);                // (held 0.7 s HOME opens the menu, and the menu stops
  check("an encoder turns (ALGORITHM picks the next track)", !same(before, after));   //  the autosave)
  a.turn(EN.ALGORITHM, -1);
}

// ---- the flash: the autosave, and the sectors the page keeps
{
  const w0 = a.ex.web_flash_writes_count();
  a.turn(EN.PRESETS, 3);                            // another sound on track 1
  a.render(25000);                                  // AUTOSAVE_IDLE 2500 ms, and AUTOSAVE_GAP 20 s from boot
  check("the autosave reaches the emulated flash", a.ex.web_flash_writes_count() > w0,
        `+${a.ex.web_flash_writes_count() - w0} writes`);
  const sectors = a.sectors();
  check("what the page keeps is the sectors that were written", Object.keys(sectors).length > 0,
        `${Object.keys(sectors).length} sectors`);

  const crc = a.ex.web_debug_obj_crc(7);            // 7 = the autosave object
  const b = await device(sectors);
  b.render(1200);
  check("a fresh instance on those sectors has the same working project",
        crc !== 0 && b.ex.web_debug_obj_crc(7) === crc, `crc 0x${(crc >>> 0).toString(16)}`);
  check("and boots on it", colours(b.screen()) > 4, `${colours(b.screen())} colours`);
}

// ---- a song file's objects, as the editor writes them
{
  const c = await device(null);
  c.render(1200);
  const cap = c.ex.web_upload_size();
  const buf = new Uint8Array(c.mem.buffer, c.ex.web_upload_buf(), cap);
  const w0 = c.ex.web_flash_writes_count();
  buf.fill(0xA5);
  const rc2 = c.ex.web_obj_put(2, 3840);            // a section (editor id 2 -> OBJ_PROJECT0), the payload's max
  const rc1 = c.ex.web_obj_put(1, 256);             // the settings
  check("a song's section object is written to the flash", rc2 === 0 && c.ex.web_flash_writes_count() > w0,
        `rc ${rc2}, +${c.ex.web_flash_writes_count() - w0} writes`);
  check("a song's settings object is written to the flash", rc1 === 0, `rc ${rc1}`);
  buf.fill(0x11);
  check("the song order goes into the settings", c.ex.web_order_put(36) === 0);
  check("an object id the flash has no place for is refused", c.ex.web_obj_put(99, 16) === -2);
  check("an object larger than the upload buffer is refused", c.ex.web_obj_put(2, cap + 1) === -1);
}

// ---- the cost: 1 s of audio against 1 s of real time (half is the project's bar for the browser build)
{
  const t = await device(null);
  t.render(500);
  const t0 = process.hrtime.bigint();
  t.render(1000);
  const ms = Number(process.hrtime.bigint() - t0) / 1e6;
  check("1 s of audio costs well under 1 s of real time", ms < 500, `${ms.toFixed(0)} ms`);
}

console.log(fails ? `\nemu: ${fails} FAILED` : "\nemu: all checks passed");
process.exit(fails ? 1 : 0);
