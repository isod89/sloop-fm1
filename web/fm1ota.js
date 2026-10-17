// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// The FM-1 USB-MIDI update protocol (M-UPGRADE compatible) over WebMIDI.
// Step 1: the running firmware reads parts of the package and stages the update
// loader. Step 2: the loader reads the whole image and writes it. In both steps
// the device sends cmd 0x30 read requests on the logical image and we answer.

const HS_QUERY = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
const UPGRADE = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7];
const MAXDATA = 512;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const IS_OTA = (id) => /^ota-/i.test(id.model);
// the update loader of SLOOP / Felucca packages ("ota-FM-1_9XX", "ota-SMK-37 Pro_9XX"; the SMK ports also under the
// Elite's and the MKE-P37's model name, Makkuro37 _7XX). A device left in the update mode of another firmware (the
// stock updater's loader) is never written: its loader may lay the image out differently. The bare "SMK-37" name
// ("ota-SMK-37_931") is ChanceTheMaker's SMK firmwares' RAM updater: another package format, so not ours.
export const OUR_LOADER = (id) => /^ota-(FM-1|SMK-37 Pro|SMK-37 Elite|MKE-P37)_[789]\d\d$/i.test(id.text);
// ChanceTheMaker's SMK firmwares (Felucca SMK, Sloop SMK, Melodee SMK, Doom): "SMK-37_9NN", their own updater
export const OTHER_SMK = (id) => /^(ota-)?SMK-37_9\d\d$/i.test(id.text);
// the SMK-37 Pro, SMK-37 Elite, MKE-P37, Starrykey 37 Play and the SMK-37+VA mod: one keyboard under several model
// names, so a package for one runs on all of them (also "SMK-37", the name other firmwares report); an FM-1 is
// another device
const SMK_FAMILY = /^(ota-)?(SMK-37([ +].*)?|MKE-P37|STARRYKEY-37.*)$/i;
export const sameKeyboard = (a, b) => a === b || (SMK_FAMILY.test(a) && SMK_FAMILY.test(b));   // two models
// an official package on the connected keyboard (the return to the M-Vave firmware): "same" (its model), "other"
// (another model of the same keyboard: allowed, the page warns), "unknown" (the keyboard does not name its model,
// as "SMK-37" or "SMK-37+VA": allowed, the page warns), "refuse" (another device: FM-1 and SMK never mix)
const NAMED = /^(SMK-37 Pro|SMK-37 Elite|MKE-P37|STARRYKEY-37 PLAY)$/i;
export function stockFit(deviceModel, product) {
  const dm = deviceModel.replace(/^ota-/i, ""), pm = product.split("_")[0];
  if (!sameKeyboard(dm, pm)) return "refuse";
  if (dm.toLowerCase() === pm.toLowerCase()) return "same";
  return NAMED.test(dm) ? "other" : "unknown";
}
// the MIDI ports find() probes: the FM-1 / SMK models' and the firmwares' names; other gear is never sent anything
export const PORT_RE = /fm-1|smk|elite|mke-p37|starry|felucca|sloop|clipper|melodee|fomni|x0x|makkuro|maku|rainbow|snoop|vector|vox|ota|composite|sinco|usb-midi/i;
// a port name without its in / out part (Windows names a device's second port pair "MIDIIN2 (X)" / "MIDIOUT2 (X)")
const portBase = (n) => n.replace(/^MIDI(?:IN|OUT)(\d*) \((.*)\)$/i, "$2 $1").replace(/[ -](?:in|out|input|output)$/i, "").trim().toLowerCase();

// errors carry a code the pages translate: notfound, model, stopped, noloader, lost, noreturn,
// mismatch (detail: the identity the device reports), badreq, foreign (detail: the loader's identity)
function fail(code, msg, detail = "") { const e = new Error(msg); e.code = code; e.detail = detail; return e; }

export function pack7(data) {
  const out = []; let acc = 0, nb = 0;
  for (const b of data) {
    acc |= b << nb; nb += 8;
    while (nb >= 7) { out.push(acc & 0x7F); acc >>>= 7; nb -= 7; }
  }
  if (nb) out.push(acc & 0x7F);
  return out;
}

export function unpack7(s) {
  const out = []; let acc = 0, nb = 0;
  for (const b of s) {
    acc |= (b & 0x7F) << nb; nb += 7;
    while (nb >= 8) { out.push(acc & 0xFF); acc >>>= 8; nb -= 8; }
  }
  return out;
}

function response(addr, data, fl = 0) {
  const len = data.length;
  const body = [0x00, 0x59, 0x30, (len + 8) & 0xFF, ((len + 8) >> 8) & 0xFF, 0, fl,
    addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF,
    len & 0xFF, (len >> 8) & 0xFF, 0, ...data];
  let s = 0;
  for (let i = 6; i < body.length; i++) s += body[i];
  body.push(~s & 0xFF);
  return [0xF0, ...pack7(body), 0xF7];
}

function parseRequest(pkt) {
  if (pkt[0] !== 0xF0 || pkt[pkt.length - 1] !== 0xF7) return null;
  const u = unpack7(pkt.slice(1, -1));
  if (u.length !== 15 || u[0] !== 0 || u[1] !== 0x59 || u[2] !== 0x30) return null;
  let s = 0;
  for (let i = 6; i < 14; i++) s += u[i];
  if ((~s & 0xFF) !== u[14]) return null;
  return { fl: u[6], addr: (u[7] | (u[8] << 8) | (u[9] << 16) | (u[10] << 24)) >>> 0, len: u[11] | (u[12] << 8) | (u[13] << 16) };
}

function parseIdentity(pkt) {
  const d = unpack7(pkt.slice(1, -1));
  if (d.length !== 34 || d[0] !== 0 || d[1] !== 0x59 || d[2] !== 0x11) return null;
  const txt = String.fromCharCode(...d.slice(6, 33)).split("\0")[0];   // (some firmwares end the text early)
  const m = /^([^_]+)_(\d+)$/.exec(txt);
  return m ? { text: txt, model: m[1], version: parseInt(m[2], 10) } : null;
}

// one MIDI in/out pair with a SysEx queue
class Link {
  constructor(input, output) {
    this.input = input; this.output = output; this.q = []; this.waiter = null;
    input.onmidimessage = (ev) => {
      if (ev.data[0] !== 0xF0) return;
      this.q.push(Array.from(ev.data));
      if (this.waiter) { const w = this.waiter; this.waiter = null; w(); }
    };
  }
  get lost() { return this.input.state === "disconnected" || this.output.state === "disconnected"; }
  // false when the port is gone (unplugged): WebMIDI throws on a disconnected output
  send(bytes) {
    try { this.output.send(bytes); return true; } catch (_) { return false; }
  }
  drain() { this.q.length = 0; }
  async read(ms) {
    if (!this.q.length) {
      let tm;
      await Promise.race([new Promise((r) => { this.waiter = r; }), new Promise((r) => { tm = setTimeout(r, ms); })]);
      clearTimeout(tm);
      this.waiter = null;
    }
    return this.q.shift() || null;
  }
  close() { this.input.onmidimessage = null; }
}

async function handshake(link, tries = 3) {
  link.drain();
  for (let t = 0; t < tries; t++) {
    if (!link.send(HS_QUERY)) return null;
    const end = Date.now() + 1000;
    while (Date.now() < end) {
      const p = await link.read(end - Date.now());
      if (!p) break;
      const id = parseIdentity(p);
      if (id) return id;
    }
  }
  return null;
}

export class Updater {
  // opts.all: probe every MIDI port, not only PORT_RE's (the handshake query is harmless to other gear)
  constructor(access, log, opts = {}) {
    this.access = access;
    this.log = log || (() => {});
    this.all = !!opts.all;
    this.said = new Set();
  }

  say(t) { if (!this.said.has(t)) { this.said.add(t); this.log(t); } }   // each port line once

  // every in/out pair with the same port name that answers the handshake
  async find(filter) {
    const outs = [...this.access.outputs.values()];
    for (const input of this.access.inputs.values()) {
      if (input.state === "disconnected") continue;
      const name = input.name || "";
      if (!this.all && !PORT_RE.test(name)) { this.say(`port "${name}": not probed`); continue; }   // never probe other gear
      const output = outs.find((o) => o.name === input.name) ||
        outs.find((o) => portBase(o.name || "") === portBase(name)) || (outs.length === 1 ? outs[0] : null);
      if (!output) { this.say(`port "${name}": no output of that name`); continue; }
      try { await input.open(); await output.open(); } catch (_) { this.say(`port "${name}": cannot open (in use?)`); continue; }
      const link = new Link(input, output);
      const id = await handshake(link, 2);
      this.say(`port "${name}" / "${output.name}": ${id ? id.text : "no answer"}`);
      if (id && (!filter || filter(id))) return { link, id, name: input.name };
      link.close();
    }
    return null;
  }

  // the MIDI inputs there are, for the "not found" message: " (MIDI: a, b)" (a port the installer skipped shows)
  seen() {
    const names = [...this.access.inputs.values()].filter((p) => p.state !== "disconnected").map((p) => p.name || "?");
    return names.length ? ` (MIDI: ${names.join(", ")})` : " (MIDI: none)";
  }

  async waitFor(filter, ms) {
    const end = Date.now() + ms;
    while (Date.now() < end) {
      const dev = await this.find(filter);
      if (dev) return dev;
      await sleep(1000);
    }
    return null;
  }

  // serve read requests until the device asks for one of the finish addresses; stops early
  // when the port disappears (lost: true) instead of waiting out idleMs
  async serve(link, image, finishAddr, idleMs, onProgress) {
    let served = 0, last = Date.now();
    for (;;) {
      const pkt = await link.read(1000);
      if (!pkt) {
        if (link.lost) return { served, finished: false, lost: true };
        if (Date.now() - last > idleMs) return { served, finished: false };
        continue;
      }
      const r = parseRequest(pkt);
      if (!r) continue;
      last = Date.now();
      if (r.addr === 0xE0000000 || r.addr === 0xF0000000) {
        link.send(response(r.addr, [...new TextEncoder().encode("success"), 0]));
        if (r.addr === finishAddr) return { served, finished: true };
        continue;
      }
      if (r.len > MAXDATA || r.addr + r.len > image.length) throw fail("badreq", `bad request ${r.addr.toString(16)}+${r.len}`);
      await sleep(10);
      if (!link.send(response(r.addr, Array.from(image.subarray(r.addr, r.addr + r.len)), r.fl))) {
        return { served, finished: false, lost: true };
      }
      served++;
      if (onProgress) onProgress(served, r.addr);
    }
  }

  // full install: running firmware -> loader -> new firmware. anyModel: the package may be for another model of the
  // same keyboard (an SMK-37 Elite package on a unit that reports SMK-37 Pro); never an FM-1 one on an SMK, nor back
  async install(image, product, onStep, anyModel = false, opts = {}) {
    const step = onStep || (() => {});
    const dev = await this.find((id) => !IS_OTA(id));
    if (!dev) throw fail("notfound", "FM-1 not found (USB cable, and no other app using it?)", this.seen());
    if (OTHER_SMK(dev.id)) { dev.link.close(); throw fail("other", `the keyboard runs a firmware with its own updater (${dev.id.text}): nothing was sent`, dev.id.text); }
    const [model] = product.split("_");
    const refusal = boardRefusal(dev.id.model, image, opts.fm1Confirmed);   // (the package's board, before anything)
    if (refusal) { dev.link.close(); throw refusal; }
    if (dev.id.model !== model && !(anyModel && sameKeyboard(dev.id.model, model))) { dev.link.close(); throw fail("model", `the device is ${dev.id.text}, the package is for ${product}`); }
    step("start", dev.id.text);
    dev.link.send(UPGRADE);
    await sleep(2000);
    const s1 = await this.serve(dev.link, image, 0xE0000000, 8000, (n) => step("verify", n));
    dev.link.close();
    // some firmwares restart into their loader at once, without step 1: such a loader (ours, of this keyboard) goes on
    const jumped = !s1.finished && s1.lost && s1.served === 0;
    if (!s1.finished && !jumped) throw fail(s1.lost ? "lost" : "stopped", `the device ${s1.lost ? "was disconnected" : "stopped"} after ${s1.served} requests: nothing was written`);
    step("loader");
    await sleep(3000);
    const ota = await this.waitFor(IS_OTA, jumped ? 15000 : 30000);
    if (jumped && !(ota && OUR_LOADER(ota.id) && sameKeyboard(ota.id.model.replace(/^ota-/i, ""), dev.id.model))) {
      if (ota) ota.link.close();
      throw fail(ota ? "foreign" : "lost", ota ? `the device restarted into the update mode of another firmware (${ota.id.text}): nothing was written`
        : "the device was disconnected after 0 requests: nothing was written", ota ? ota.id.text : "");
    }
    if (!ota) throw fail("noloader", "the update loader did not appear. Replug the USB cable and press Install again: the device stays in update mode until it is finished.");
    const s2 = await this.write(ota, image, step);
    if (!s2.finished) throw fail(s2.lost ? "lost" : "stopped", `the loader ${s2.lost ? "was disconnected" : "stopped"} after ${s2.served} requests. Replug and press Install again to resume.`);
    step("reboot");
    await sleep(3000);
    const back = await this.waitFor((id) => !IS_OTA(id), 40000);
    if (!back) throw fail("noreturn", "the device did not come back: power-cycle it");
    back.link.close();
    if (back.id.text !== product) throw fail("mismatch", `installed, but the device reports ${back.id.text}`, back.id.text);
    step("done", back.id.text);
    return back.id.text;
  }

  // step 2 with the loader found by find()/waitFor()
  async write(ota, image, step) {
    ota.link.send(UPGRADE);
    await sleep(2000);
    const s2 = await this.serve(ota.link, image, 0xF0000000, 180000, (n) => step("write", n));
    ota.link.close();
    return s2;
  }

  // resume: the device is already in update mode (loader) -> true when the write finished. Only SLOOP's
  // own loader is resumed, except for the return to the official firmware (opts.product: the official
  // package, its identity checked once the FM-1 is back, after Felucca 1.0): that one is the official
  // loader's own image, as M-UPGRADE would write it. opts.package: the identity of the package being
  // written; the loader's model must match it (or opts.product's) by install()'s rule (the same model, or with
  // opts.anyModel another model of the same keyboard), and without either identity nothing is written
  async resume(image, onStep, opts = {}) {
    const ota = await this.find(IS_OTA);
    if (!ota) return false;
    if (!OUR_LOADER(ota.id) && !opts.product) {
      ota.link.close();
      throw fail("foreign", `the FM-1 is in the update mode of another firmware (${ota.id.text}): finish that update with its own updater`, ota.id.text);
    }
    const want = opts.product || opts.package || "";
    const refusal = boardRefusal(ota.id.model, image, opts.fm1Confirmed);
    if (refusal) { ota.link.close(); throw refusal; }
    const [lm, pm] = [ota.id.model.replace(/^ota-/i, ""), want.split("_")[0]];
    if (!want || (lm !== pm && !(opts.anyModel && sameKeyboard(lm, pm)))) {
      ota.link.close();
      throw fail("model", `the device is in the update mode of ${ota.id.text}, the package is for ${want || "an unknown model"}`, ota.id.text);
    }
    const step = onStep || (() => {});
    if (!(await this.write(ota, image, step)).finished) return false;
    if (opts.product) {
      step("reboot");
      await sleep(3000);
      const back = await this.waitFor((id) => !IS_OTA(id), 40000);
      if (!back) throw fail("noreturn", "the FM-1 did not come back after the write: power-cycle it");
      back.link.close();
      if (back.id.text !== opts.product) throw fail("mismatch", `written, but the FM-1 reports ${back.id.text}`, back.id.text);
      step("done", back.id.text);
    }
    return true;
  }
}

// ---- the package's board: read from the package itself, never from an identity ----
// The FM-1's and the SMK-37's official packages share the head (0x0-0x3FFF) and the update loader, and a third-party
// FM-1 firmware running on an SMK-37 reports an FM-1 identity: no identity tells the board. Installed on an SMK-37,
// the FM-1's stock package erases the factory calibration (BTIF / USRTRIM 0xC0000-0xC1FFF) and writes the FM-1's
// USR image over the SMK's (0xF4000). So the board comes from the package: flash.bin's length (SMK-37 0x9D000,
// FM-1 0x93000), its region table (app offset 0x60 = flash 0x4060-0x411F, SFC-enciphered with the chip key 0x980F:
// BTIF and USR at 0xC0000 / 0xF4000 on the SMK-37, 0xE9000 / 0xEA000 on the FM-1) and a USR image's place (UFW
// entry 50).
export const FM1_STOCK_MSG = "This is the FM-1 firmware. Your keyboard is an SMK-37? Don't install it.";
export const FM1_CONFIRM_MSG = FM1_STOCK_MSG + " On an SMK-37 it erases the keyboard's calibration. Install it only on an FM-1." +
  " SMK-37 owners whose keyboard runs an FM-1 firmware (it then reports an FM-1): use the SMK-37 package from M-Vave, never this one.";
const LAYOUT = { smk: { len: 0x9D000, btif: 0xC0000, usr: [0xF4000, 0xA000] }, fm1: { len: 0x93000, btif: 0xE9000, usr: [0xEA000, 0x12000] } };
const LOADER_MARK = [..."FELUCCA-LOADER-1"].map((c) => c.charCodeAt(0));   // our update loaders (crt0_ldr.S)
const SMK_MODEL = /^(ota-)?(SMK-37([ +].*)?|MKE-P37|STARRYKEY-37.*)$/i;      // an identity of the SMK-37 keyboard
const le = (a, o, n) => { let v = 0; for (let i = n - 1; i >= 0; i--) v = v * 256 + a[o + i]; return v; };
function ufwCipher(b) {                    // the UFW header cipher (key 0xFFFF, its own inverse)
  const out = Uint8Array.from(b);
  for (let i = 0, k = 0xFFFF; i < out.length; i++) { out[i] ^= k & 0xFF; k = ((k << 1) ^ (k & 0x8000 ? 0x1021 : 0)) & 0xFFFF; }
  return out;
}
function sfcCipher(b, base, key = 0x980F) {   // the JieLi SFC cipher of the app area (32-byte blocks, its own inverse)
  const out = Uint8Array.from(b);
  for (let i = 0; i < out.length; i += 32) {
    let k = (key ^ ((base + i) >>> 2)) & 0xFFFF;
    for (let j = i; j < i + 32 && j < out.length; j++) { out[j] ^= k & 0xFF; k = ((k << 1) ^ (k & 0x8000 ? 0x1021 : 0)) & 0xFFFF; }
  }
  return out;
}
// the package's files: { type, off, size, at, len } (at / len: where a USR image goes)
export function packageFiles(image) {
  if (image.length < 0x40) return [];
  const h = ufwCipher(image.subarray(0, 0x40)), n = le(h, 8, 2), out = [];
  for (let i = 0; i < n && i < 11 && 0x90 + i * 0x50 <= image.length; i++) {
    const e = ufwCipher(image.subarray(0x40 + i * 0x50, 0x90 + i * 0x50));
    out.push({ type: le(e, 0, 2), off: le(e, 8, 4), size: le(e, 12, 4), at: le(e, 0x1C, 4), len: le(e, 0x20, 4) });
  }
  return out;
}
// flash.bin's region table: { VM: [address, size], PRCT, BTIF, USR, ... }
export function regionTable(flash) {
  const t = {};
  if (flash.length < 0x4120) return t;
  const d = sfcCipher(flash.subarray(0x4060, 0x4120), 0x60);
  for (let r = 0; r < 0xC0; r += 32) {
    let name = "";
    for (let i = 16; i < 32 && d[r + i] > 0x20 && d[r + i] < 0x7F; i++) name += String.fromCharCode(d[r + i]);
    if (name) t[name] = [le(d, r + 4, 4), le(d, r + 8, 4)];
  }
  return t;
}
// "smk", "fm1" (any sign of the FM-1's layout) or null (not a package, or another layout)
export function packageBoard(image) {
  const files = packageFiles(image), fl = files.find((f) => f.type === 0);
  if (!fl || fl.off + fl.size > image.length) return null;
  const rt = regionTable(image.subarray(fl.off, fl.off + fl.size)), usr = files.filter((f) => f.type === 50);
  const S = LAYOUT.smk, F = LAYOUT.fm1;
  if (fl.size === S.len && rt.BTIF && rt.BTIF[0] === S.btif && rt.USR && rt.USR[0] === S.usr[0] && rt.USR[1] === S.usr[1] &&
      usr.every((u) => u.at === S.usr[0] && u.len === S.usr[1])) return "smk";
  if (fl.size === F.len || (rt.BTIF && rt.BTIF[0] === F.btif) || (rt.USR && rt.USR[0] === F.usr[0]) || usr.some((u) => u.at === F.usr[0])) return "fm1";
  return null;
}
function hasOurLoader(image) {
  const m = LOADER_MARK;
  for (let i = image.indexOf(m[0]); i >= 0 && i + m.length <= image.length; i = image.indexOf(m[0], i + 1)) {
    let j = 1;
    while (j < m.length && image[i + j] === m[j]) j++;
    if (j === m.length) return true;
  }
  return false;
}
// a stock package (M-Vave's, or any without our update loader): one with flash.bin whose loader is not ours
export const isStockPackage = (image) => packageFiles(image).some((f) => f.type === 0) && !hasOurLoader(image);
// null, or why image must not be written to a device that reports model (an Error with a code, nothing sent yet):
// "fm1pkg" an FM-1 layout for an SMK-37; "board" a stock package not laid out for the SMK-37 on one; "fm1confirm" a
// stock package not laid out for the SMK-37 on a device that reports another model (an FM-1, or an SMK-37 under an
// FM-1 firmware): only when the user confirmed the device is an FM-1 (fm1Confirmed)
export function boardRefusal(model, image, fm1Confirmed = false) {
  const board = packageBoard(image), smk = SMK_MODEL.test(model);
  if (smk && board === "fm1") return fail("fm1pkg", `${FM1_STOCK_MSG} (The device reports ${model}.) Nothing was sent.`);
  if (!isStockPackage(image) || board === "smk") return null;
  if (smk) return fail("board", `this package is not laid out for the SMK-37 (flash.bin, region table): nothing was sent`);
  return fm1Confirmed ? null : fail("fm1confirm", FM1_CONFIRM_MSG);
}
