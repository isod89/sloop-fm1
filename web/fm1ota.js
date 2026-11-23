// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// The FM-1 USB-MIDI update protocol (M-UPGRADE compatible) over WebMIDI.
// Step 1: the running firmware reads parts of the package and stages the update
// loader. Step 2: the loader reads the whole image and writes it. In both steps
// the device sends cmd 0x30 read requests on the logical image and we answer.

const HS_QUERY = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
const INFO = [0xF0, 0x7D, 0x46, 0x4C, 0x01, 0xF7];
const RELEASE = /^(?:FELUCCA )?(?:SLOOP )?(\d+(?:\.\d+)+(?:[-+][A-Za-z0-9_.+-]+)?)$/;
const UPGRADE = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7];
const MAXDATA = 512;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const IS_OTA = (id) => /^ota-/i.test(id.model);
// the update loader of SLOOP / Felucca packages ("ota-FM-1_9XX"). A device left in the update mode of another
// firmware (the stock updater's loader) is never written: its loader may lay the image out differently.
export const OUR_LOADER = (id) => /^ota-FM-1_9\d\d$/i.test(id.text);
// the MIDI ports the installer may probe (never other gear): the FM-1 under SLOOP / Felucca ("Felucca"), under
// CLIPPER ("Clipper", its sister firmware), under a firmware named after itself ("SLOOP"), the update loaders, and
// the names Windows / macOS give the stock firmware
export const FM1_PORT = /fm-1|felucca|sloop|clipper|ota|composite|sinco|usb-midi/i;
// Windows can expose the update loader without the product name.
const plausible = (port) => FM1_PORT.test(port.name || "") || /usb midi/i.test(port.name || "");
const portKey = (name) => name.replace(/^midi(?:in|out)\d* \((.*)\)$/i, "$1").replace(/\s+\d+$/, "").toLowerCase();
const samePair = (a, b) => a && b && a[0] === b[0] && a[1] === b[1];
const ambiguous = () => fail("ambiguous", "The FM-1 MIDI target is ambiguous. Connect only the intended FM-1 and press Install again.");


// errors carry a code the pages translate: notfound, model, stopped, noloader, lost, noreturn,
// mismatch (detail: the identity the device reports), badreq, ambiguous, foreign (detail: the loader's identity)
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
  if (pkt[0] !== 0xF0 || pkt.at(-1) !== 0xF7) return null;
  const d = unpack7(pkt.slice(1, -1));
  if (d.length !== 34 || d[0] !== 0 || d[1] !== 0x59 || d[2] !== 0x11) return null;
  const txt = String.fromCharCode(...d.slice(6, 33)).replace(/\0+$/, "");
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
  // Keep the ports open for the operation, so a same-port mode change can reuse them.
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

function infoRelease(packet) {
  if (!INFO.slice(0, -1).every((byte, i) => packet[i] === byte) || packet.at(-1) !== 0xF7) return null;
  const data = packet.slice(5, -1);
  if (data.some((byte) => byte > 0x7F)) return null;
  const nul = data.indexOf(0);
  if (nul < 1 || nul > 24 || data.length < nul + 6) return null;
  const version = RELEASE.exec(String.fromCharCode(...data.slice(0, nul)))?.[1];
  const engineCount = data[nul + 1];
  if (!version || !engineCount) return null;
  let pos = nul + 6; // version, engine count and four more INFO fields
  for (let i = 0; i < engineCount; i++) {
    const end = data.indexOf(0, pos);
    if (end <= pos || end - pos > 8) return null;
    pos = end + 1;
  }
  // Older apps may omit the track count and editor protocol version.
  return data.length - pos <= 2 ? version : null;
}

function expectedRelease(product, release = null) {
  const version = release === null ? null : RELEASE.exec(release)?.[1];
  if (!/^FM-1_\d{3}$/.test(product || "") || (release !== null && !version)) {
    throw fail("metadata", "The update requires the expected FM-1 product and a valid release, if specified.");
  }
  return version;
}

export class Updater {
  constructor(access, log) {
    this.access = access;
    this.log = log || (() => {});
  }

  async withPorts(operation) {
    let portError;
    const context = {
      opened: new Set(),
      inspect: () => {
        if (portError) throw portError;
        const inventory = this.inventory();
        context.onInventory?.(inventory);
        return inventory;
      },
    };
    const observe = () => {
      try { context.inspect(); } catch (error) { portError = error; }
    };
    this.access.addEventListener?.("statechange", observe);
    try {
      return await operation(context);
    } finally {
      this.access.removeEventListener?.("statechange", observe);
      await Promise.allSettled([...context.opened].map(async (port) => port.close?.()));
    }
  }

  inventory() {
    const candidates = (ports) => [...ports.values()].filter((p) => p.state !== "disconnected" && plausible(p));
    const inputs = candidates(this.access.inputs), outputs = candidates(this.access.outputs);
    if (inputs.length > 1 || outputs.length > 1) throw ambiguous();
    if (!inputs.length || !outputs.length) return { inputs, outputs };
    if (portKey(inputs[0].name) !== portKey(outputs[0].name)) throw ambiguous();
    return { inputs, outputs, pair: [inputs[0], outputs[0]] };
  }

  async probe(pair, context, transitioning = false) {
    try {
      for (const port of pair) {
        if (port.connection !== "open") {
          context.opened.add(port);
          await port.open();
        }
      }
    } catch (_) { return null; }
    const link = new Link(...pair);
    let keep = false;
    try {
      const id = await handshake(link, 2);
      const current = samePair(pair, context.inspect().pair);
      if (!current && !transitioning) throw ambiguous();
      if (current && id) {
        keep = true;
        return { link, id, pair };
      }
      return null;
    } finally {
      if (!keep) link.close();
    }
  }

  async select(context) {
    const { pair, inputs, outputs } = context.inspect();
    if (pair) return this.probe(pair, context);
    if (inputs.length || outputs.length) throw ambiguous();
    return null;
  }

  active(dev, context) {
    const { pair } = context.inspect();
    if (pair && !samePair(pair, dev.pair)) throw ambiguous();
    return samePair(pair, dev.pair) && !dev.link.lost;
  }

  // the MIDI inputs there are, for the "not found" message: " (MIDI: a, b)" (a port the installer skipped shows)
  seen() {
    const names = [...this.access.inputs.values()].filter((p) => p.state !== "disconnected").map((p) => p.name || "?");
    return names.length ? ` (MIDI: ${names.join(", ")})` : " (MIDI: none)";
  }

  async waitFor(previous, filter, ms, context, check) {
    let ready = Date.now() + 3000;
    const end = ready + ms;
    let observed;
    context.onInventory = ({ pair, inputs, outputs }) => {
      if (pair && !samePair(pair, previous.pair)) {
        if (inputs.includes(previous.pair[0]) || outputs.includes(previous.pair[1])) throw ambiguous();
        if (observed && !samePair(pair, observed)) throw ambiguous();
        // Give the replacement pair time to settle while watching for competing units.
        if (!observed && check) ready = Date.now() + 3000;
        observed = pair;
      } else if (pair && observed) {
        throw ambiguous();
      }
    };
    try {
      // Observe changes during USB settle time as well as during the search.
      while (Date.now() < end) {
        const { pair } = context.inspect();
        if (pair && Date.now() >= ready) {
          const dev = await this.probe(pair, context, true);
          if (dev) {
            let keep = false;
            try {
              if (filter(dev.id)) {
                if (check) await check(dev);
                if (this.active(dev, context)) {
                  keep = true;
                  return dev;
                }
              } else if (!samePair(pair, previous.pair)) {
                throw ambiguous();
              }
            } finally {
              if (!keep) dev.link.close();
            }
          }
          await sleep(1000);
        } else {
          await sleep(100);
        }
      }
      return null;
    } finally {
      context.onInventory = null;
    }
  }

  // serve read requests until the device asks for one of the finish addresses; stops early
  // when the port disappears (lost: true) instead of waiting out idleMs
  async serve(dev, image, finishAddr, idleMs, onProgress, context) {
    const { link } = dev;
    let served = 0, last = Date.now();
    for (;;) {
      const pkt = await link.read(1000);
      if (!pkt) {
        if (!this.active(dev, context)) return { served, finished: false, lost: true };
        if (Date.now() - last > idleMs) return { served, finished: false };
        continue;
      }
      const r = parseRequest(pkt);
      if (!r) continue;
      if (!this.active(dev, context)) return { served, finished: false, lost: true };
      last = Date.now();
      if (r.addr === 0xE0000000 || r.addr === 0xF0000000) {
        if (!link.send(response(r.addr, [...new TextEncoder().encode("success"), 0]))) {
          return { served, finished: false, lost: true };
        }
        if (r.addr === finishAddr) return { served, finished: true };
        continue;
      }
      if (r.len > MAXDATA || r.addr + r.len > image.length) throw fail("badreq", `bad request ${r.addr.toString(16)}+${r.len}`);
      await sleep(10);
      if (!this.active(dev, context) || !link.send(response(r.addr, Array.from(image.subarray(r.addr, r.addr + r.len)), r.fl))) {
        return { served, finished: false, lost: true };
      }
      served++;
      if (onProgress) onProgress(served, r.addr);
    }
  }

  // The page can also resume a loader using the same selection and live link.
  async install(image, product, onStep, { resume = false, release } = {}) {
    const step = onStep || (() => {});
    const version = expectedRelease(product, release);
    return this.withPorts(async (context) => {
      const dev = await this.select(context);
      if (!dev) throw fail("notfound", "FM-1 not found (USB cable, and no other app using it?)", this.seen());
      if (IS_OTA(dev.id) && resume) {
        await this.resumeLoader(dev, image, step, context, { product, release: version, official: false });
        return true;
      }
      const [model] = product.split("_");
      if (dev.id.model !== model) { dev.link.close(); throw fail("model", `the device is ${dev.id.text}, the package is for ${product}`); }
      step("start", dev.id.text);
      const s1 = await this.transfer(dev, image, 0xE0000000, 8000, (n) => step("verify", n), context);
      if (!s1.finished) throw fail(s1.lost ? "lost" : "stopped", `the device ${s1.lost ? "was disconnected" : "stopped"} after ${s1.served} requests: nothing was written`);
      step("loader");
      const ota = await this.waitFor(dev, IS_OTA, 60000, context);
      if (!ota) throw fail("noloader", "the update loader did not appear. Replug the USB cable and press Install again: the device stays in update mode until it is finished.", this.seen());
      const s2 = await this.write(ota, image, step, context);
      if (!s2.finished) throw fail(s2.lost ? "lost" : "stopped", `the loader ${s2.lost ? "was disconnected" : "stopped"} after ${s2.served} requests. Replug and press Install again to resume.`);
      return this.verify(ota, product, step, context, release === undefined ? null : { release: version });
    });
  }

  async transfer(dev, image, finish, idle, onProgress, context) {
    try {
      if (!this.active(dev, context) || !dev.link.send(UPGRADE)) return { served: 0, finished: false, lost: true };
      await sleep(2000);
      return await this.serve(dev, image, finish, idle, onProgress, context);
    } finally {
      dev.link.close();
    }
  }

  async write(ota, image, step, context, officialProduct) {
    // The official loader is allowed only when the caller requests its own official package.
    const official = /^FM-1_0\d\d$/i.test(officialProduct || "") &&
      ota.id.text.toLowerCase() === `ota-${officialProduct}`.toLowerCase();
    if (!OUR_LOADER(ota.id) && !official) {
      ota.link.close();
      throw fail("foreign", `the FM-1 is in the update mode of another firmware (${ota.id.text}): finish that update with its own updater`, ota.id.text);
    }
    return this.transfer(ota, image, 0xF0000000, 180000, (n) => step("write", n), context);
  }

  async readAppRelease(dev, context) {
    const { link } = dev;
    const deadline = Date.now() + 8000;
    link.drain();
    while (Date.now() < deadline && this.active(dev, context)) {
      if (!link.send(INFO)) throw fail("lost", "The USB connection was lost while checking the app.");
      const attempt = Math.min(deadline, Date.now() + 750);
      while (Date.now() < attempt) {
        const packet = await link.read(attempt - Date.now());
        if (!this.active(dev, context)) throw fail("lost", "The USB connection was lost while checking the app.");
        if (!packet) break;
        const version = infoRelease(packet);
        if (version) return version;
      }
    }
    throw fail("noinfo", "The FM-1 did not answer the editor INFO query.");
  }

  async verify(ota, product, step, context, app) {
    step("reboot");
    let observed;
    const back = await this.waitFor(ota, (id) => !IS_OTA(id), 60000, context, async (dev) => {
      if (dev.id.text !== product) throw fail("mismatch", `written, but the FM-1 reports ${dev.id.text}`, dev.id.text);
      if (!app) return;
      observed = await this.readAppRelease(dev, context);
      if (app.release && observed !== app.release) {
        throw fail("mismatch", `written, but the FM-1 reports SLOOP ${observed}`, `SLOOP ${observed}`);
      }
    });
    if (!back) throw fail("noreturn", "the FM-1 did not come back: power-cycle it");
    back.link.close();
    if (!this.active(back, context)) throw fail("lost", "The USB connection was lost while checking the app.");
    if (app && !app.release) step("booted", `SLOOP ${observed}`);
    else step("done", app ? `SLOOP ${observed}` : back.id.text);
    return back.id.text;
  }

  async resumeLoader(ota, image, step, context, { product, release, official }) {
    const result = await this.write(ota, image, step, context, official ? product : undefined);
    if (!result.finished) throw fail(result.lost ? "lost" : "stopped", "The loader transfer did not finish. Replug and press Install again.");
    return this.verify(ota, product, step, context, official ? null : { release });
  }

  // The official recovery caller must explicitly authorize the matching stock loader.
  async resume(image, onStep, { product, release, official = false } = {}) {
    const step = onStep || (() => {});
    const version = expectedRelease(product, release);
    if (official === true && !/^FM-1_0\d\d$/.test(product)) {
      throw fail("metadata", "Official recovery requires an official FM-1 product.");
    }
    return this.withPorts(async (context) => {
      const ota = await this.select(context);
      if (!ota) return false;
      if (!IS_OTA(ota.id)) { ota.link.close(); return false; }
      await this.resumeLoader(ota, image, step, context, { product, release: version, official: official === true });
      return true;
    });
  }
}
