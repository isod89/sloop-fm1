// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// Node checks of the web pages' JS (no browser, no hardware). Run from the repo root:
//   node web/test_web.mjs
// - editor.html: the protocol section (between PROTO-BEGIN/END) against its mock device (v1 commands,
//   the user preset bank / librarian, library files, live pushes, older-firmware fallback, the v3 tracks
//   and the mixer), its tab layout and ja/en strings,
//   and the user-sample pipeline byte for byte against tools/sampleio.py
// - fm1pkg.js: productOf and logicalImage on build/felucca.fwsc (skipped without a build)
// - fm1ota.js: a full install and an unplug during the write against a simulated FM-1

import { execFileSync } from "node:child_process";
import { existsSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import vm from "node:vm";
import { logicalImage, productOf } from "./fm1pkg.js";
import { Updater, pack7, unpack7, FM1_PORT } from "./fm1ota.js";

let failed = 0;
const ok = (cond, what) => { console.log(`${what.padEnd(64)} ${cond ? "ok" : "FAIL"}`); if (!cond) failed++; };
const eq = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
const PYTHON = process.env.PYTHON || (process.platform === "win32" ? "python" : "python3");
const py = (code, ...args) => execFileSync(PYTHON, ["-c", code, ...args], { maxBuffer: 1 << 26 });
const HERE = fileURLToPath(new URL(".", import.meta.url));

/* ------------------------------------------------------------ editor protocol --- */
const html = readFileSync(join(HERE, "editor.html"), "utf8");
const proto = html.slice(html.indexOf("/*PROTO-BEGIN*/"), html.indexOf("/*PROTO-END*/"));
const E = vm.runInNewContext(proto + `
;({ frame, unframe, parse, req, Link, parseWav, resample, normalize, rootFromName, buildSlot, makeMockDevice, CMD, SMP,
   CHOP, chopNovelty, chopHits, chopSnap, chopGrid, chopEqual, chopList, chopPick, chopFit, chopZones, wavFile, zipStore, crc32,
   UP, bank, capturePatch, auditionPatch, startWatch, libraryFile, readLibraryFile, paramKeys, patternFromSteps, stepsFromPattern, upName,
   mixer, GM_DRUM, drumName, parseNotes, fmtValue, F, DRUM_LANES, LV_NAMES, emptyDrum, lockable, MICRO, FC, fillGet, fillSet, pack7, unpack7,
   backupCapture, backupRestore, backupObjects, b64enc, b64dec, FM6, KIT, kitPad, kitZones, kitLen, kitBytes, kitFit, kitSplit, kitLaneOf, kitPlace, zipRead, ROLL, SMF, ARR, mockSettings, fm6CartWrite, fm6CartPreset, fm6CartSlots, fm6CartPresets, DSYN, DSYN_MOCK_NAMES, DSYN_MOCK_KITS, SetGate, bkProjFormat, bkDevFormat, bkFwAtLeast })`,
{ setTimeout, clearTimeout, setInterval, clearInterval, console, TextEncoder, TextDecoder, Blob, Response, DecompressionStream });

async function editorMock() {
  const m = E.makeMockDevice();
  const inp = [...m.access.inputs.values()][0], out = [...m.access.outputs.values()][0];
  const link = new E.Link((d) => out.send(d), { timeout: 300 });
  inp.onmidimessage = (e) => link.receive(e.data);
  const rq = async (r, o) => link.request(r, o);
  const info = E.parse[E.CMD.INFO](await rq(E.req.info()));
  ok(info.nengines === 12 && info.engines[9] === "FM6" && info.engines[10] === "PHYS" && info.engines[11] === "NOISE" && info.engines[5] === "VOICE" && info.engines[6] === "TRIO" && info.engines[7] === "WHEEL" && info.engines[8] === "GRAIN" && info.pcount === 71 && info.pe0 === 63 && info.engines[4] === "SAMPLE",
    "editor: INFO");
  let descs = 0;
  for (let i = 0; i < info.pcount; i++) if (E.parse[E.CMD.DESC](await rq(E.req.desc(0, i))).label) descs++;
  ok(descs === info.pcount, "editor: DESC for every parameter");
  {
    /* the SLICER (core.h P_SLCR..P_SLDEPTH = 45..48, just before P_E0): the mock as params.c has it,
       and a factory preset turns it off as ui.c apply_preset_to does */
    const pc = readFileSync(join(HERE, "../firmware/src/params.c"), "utf8");
    const sd = [];
    for (let i = 45; i < 49; i++) sd.push(E.parse[E.CMD.DESC](await rq(E.req.desc(0, i))));
    ok(sd.map((d) => d.label).join() === "SLCR,PAT,RATE,DEPTH" && sd[0].names.join() === "OFF,GATE,STUT"
      && sd[2].names.join() === "1/8,1/16,1/32,8T,16T,32T" && sd[2].def === 1 && sd[1].min === 1 && sd[1].max === 16 && sd[3].def === 127
      && /\[P_SLCR\] = PE\("SLCR", N_SLCR, 0\)/.test(pc) && /\[P_SLPAT\] = PD\("PAT", F_INT, 1, 16, 1\)/.test(pc)
      && /\[P_SLRATE\] = PE\("RATE", N_SLDIV, 1\)/.test(pc) && /\[P_SLDEPTH\] = PD\("DEPTH", F_PCT, 0, 127, 127\)/.test(pc)
      && /N_SLDIV\[\] = \{"1\/8", "1\/16", "1\/32", "8T", "16T", "32T"\}/.test(pc),
      "editor: SLICER parameters 45..48 (mock == params.c)");
    await rq(E.req.set(0, 45, 2));
    await rq(E.req.set(0, 46, 7));
    const on = E.parse[E.CMD.DUMP](await rq(E.req.dump()), info);
    await rq(E.req.preset(0, 1));
    const off = E.parse[E.CMD.DUMP](await rq(E.req.dump()), info);
    ok(on.p[45] === 2 && on.p[46] === 7 && off.p[45] === 0 && off.p[46] === 1, "editor: a factory preset turns the SLICER off");
  }
  const scale = E.parse[E.CMD.DESC](await rq(E.req.desc(0, 26)));
  const scaleNames = ["CHR", "MAJ", "MIN", "DOR", "MIX", "PEN", "MPEN", "HARM", "PHRY", "LYD", "LOC", "MEL", "BLUES", "WHOLE", "DIMHW", "DIMWH"];
  ok(scale.label === "SCL" && scale.max === 15 && eq(scale.names, scaleNames), "editor: all 16 scale names exposed");
  const scaleSet = E.parse[E.CMD.SET](await rq(E.req.set(0, scale.id, 15)));
  ok(scaleSet.value === 15, "editor: new scale selection is not clamped to the old range");
  const dump = E.parse[E.CMD.DUMP](await rq(E.req.dump()), info);
  ok(dump.p.length === info.pcount && dump.g.length === info.gcount, "editor: DUMP");
  const set = E.parse[E.CMD.SET](await rq(E.req.set(0, 3, 500)));
  ok(set.value === 127, "editor: SET clamps to the range");
  const st = E.parse[E.CMD.STEP_SET](await rq(E.req.stepSet(5, { n: 2, notes: [60, 64], time: 0, flags: 1, vel: 100 })));
  ok(st.n === 2 && st.notes[1] === 64 && st.vel === 100, "editor: STEP_SET");
  const pj = E.parse[E.CMD.PROJECT](await rq(E.req.project(1, 2), { timeout: 4000, retries: 0 }));
  ok(pj.used === 1, "editor: PROJECT save");
  /* sample upload as smpUpload() does it */
  const s = Int16Array.from({ length: 3000 }, (_, i) => Math.round(8000 * Math.sin(i / 7)));
  const { hdr, data } = E.buildSlot("test", [{ s, root: 60 }]);
  let rc = E.parse[E.CMD.SMP_BEGIN](await rq(E.req.smpBegin(1), { timeout: 1000, retries: 0 })).rc;
  for (let off = 0; off < data.length && !rc; off += 256) {
    rc = E.parse[E.CMD.SMP_WRITE](await rq(E.req.smpWrite(1, E.SMP.DATA_OFF + off, data.subarray(off, off + 256)), { timeout: 1000 })).rc;
  }
  rc = rc || E.parse[E.CMD.SMP_END](await rq(E.req.smpEnd(1, hdr), { timeout: 2000, retries: 0 })).rc;
  const si = E.parse[E.CMD.SMP_INFO](await rq(E.req.smpInfo()));
  ok(rc === 0 && si.slots[1].zones === 1 && si.slots[1].name === "TEST", "editor: sample upload (CRC checked by the mock)");
  /* a device that never answers */
  const dead = new E.Link(() => {}, { timeout: 30 });
  const err = await dead.request(E.req.info(), { retries: 1 }).then(() => null, (e) => e.message);
  ok(/^timeout/.test(err || ""), "editor: no reply -> timeout after the retries");
  link.close();
  m.stop();
}

/* ------------------------------------- editor protocol v2: librarian + live --- */
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const js = (x) => JSON.stringify(x);
/* a mock + Link pair; counts frames sent per cmd and timeouts reported */
function attachMock(opt, linkOpt = {}) {
  const m = E.makeMockDevice({ auto: false, ...opt });
  const inp = [...m.access.inputs.values()][0], out = [...m.access.outputs.values()][0];
  const sent = {}, ev = { timeouts: 0, unknown: [], pushes: [] };
  const link = new E.Link((d) => { sent[d[4]] = (sent[d[4]] || 0) + 1; out.send(d); }, {
    timeout: 300, onTimeout: () => ev.timeouts++, onUnknown: (f) => ev.unknown.push(f),
    onPush: (f) => ev.pushes.push({ ...f, pending: link.cur ? link.cur.cmd : 0 }), ...linkOpt });
  inp.onmidimessage = (e) => link.receive(e.data);
  const rq = (r, o) => link.request(r, o);
  return { m, link, rq, sent, ev, done: () => { link.close(); m.stop(); } };
}
const emptyStep = { n: 0, notes: [0, 0, 0, 0], time: 2, flags: 0, vel: 0 };

async function editorLibrarian() {
  const { m, rq, done } = attachMock({});
  const C = E.CMD;
  const info = E.parse[C.INFO](await rq(E.req.info()));
  const pdesc = [];
  for (let i = 0; i < info.pcount; i++) pdesc.push(E.parse[C.DESC](await rq(E.req.desc(0, i))));
  const keys = E.paramKeys(pdesc, info.pe0, info.pcount);
  ok(keys[6] === "PIT" && keys[13] === "PIT#2" && keys[info.pe0] === "E0" && new Set(keys).size === keys.length, "librarian: parameter keys unique (label#n, E0..E7)");

  const b = await E.bank.list(rq);
  ok(b.total === 32 && b.slots.length === 32 && b.slots[1].used && b.slots[1].name === "GLASS BELL" && !b.slots[3].used
    && b.slots[31].slot === 31, "librarian: UP_LIST, 32 slots in 2 frames");

  const cap = (await E.capturePatch(rq, info, "acid test")).patch;
  ok(cap.engine === 0 && cap.p.length === info.pcount && cap.pattern && cap.pattern[0][0] === 45 && cap.pattern[0][1] === 1,
    "librarian: capture = DUMP + first 16 steps");
  let rc = await E.bank.put(rq, 10, cap);
  const g = await E.bank.get(rq, info, 10);
  ok(rc === 0 && g.used && g.name === "acid test" && g.engine === 0 && eq(g.p, cap.p) && js(g.pattern) === js(cap.pattern),
    "librarian: UP_PUT -> UP_GET round trip");
  const bad = E.parse[C.UP_PUT](await rq(E.req.upPut(60, cap), { timeout: 2500, retries: 0 }));
  ok(bad.rc === 1, "librarian: UP_PUT to a slot past the bank -> rc 1");
  ok(E.upName("") === "PATCH" && E.upName("abcdefghijklmnop") === "abcdefghijkl" && E.upName("Bäss") === "Bss", "librarian: device names (ASCII, 1..12)");

  await rq(E.req.set(0, 1, 33));
  rc = await E.bank.store(rq, 11, "STORED");
  const g2 = await E.bank.get(rq, info, 11);
  const d1 = E.parse[C.DUMP](await rq(E.req.dump()), info);
  ok(rc === 0 && g2.name === "STORED" && g2.engine === d1.engine && eq(g2.p, d1.p) && g2.p[1] === 33, "librarian: UP_STORE keeps the current sound");

  /* another engine, empty sequencer, then UP_LOAD brings the sound back, not the pattern (LIVE) */
  await rq(E.req.preset(2, 0));
  for (let i = 0; i < info.nstep; i++) await rq(E.req.stepSet(i, emptyStep));
  rc = await E.bank.load(rq, 10);
  const d2 = E.parse[C.DUMP](await rq(E.req.dump()), info);
  const st = [];
  for (let i = 0; i < 16; i++) st.push(E.parse[C.STEP_GET](await rq(E.req.stepGet(i))));
  ok(rc === 0 && d2.engine === 0 && eq(d2.p, cap.p) && st.every((x) => !x.n), "librarian: UP_LOAD applies the sound, the sequencer stays empty (LIVE)");

  rc = await E.bank.erase(rq, 11);
  const b2 = await E.bank.list(rq);
  const rcEmpty = await E.bank.load(rq, 11);
  ok(rc === 0 && !b2.slots[11].used && b2.slots[10].used && rcEmpty === 1, "librarian: UP_ERASE, UP_LOAD of an empty slot -> rc 1");

  /* audition: a PHASE patch with a pattern into an empty sequencer: the sound only (as UP_LOAD in SLOOP), never
     the pattern, and the mix / pattern / key parameters stay (ui.c param_kept) */
  const bass = await E.bank.get(rq, info, 2);   /* PHASE RESO, with the ACID pattern */
  for (let i = 0; i < info.nstep; i++) await rq(E.req.stepSet(i, emptyStep));
  const KEPT = [0, 25, 26, 27, 29, 30, 31, 32, 39, 40, 49];
  for (const [id, v] of [[0, 77], [25, 5], [29, 12], [39, -10], [49, 2]]) await rq(E.req.set(0, id, v));
  const d2b = E.parse[C.DUMP](await rq(E.req.dump()), info);
  const flashBefore = js(m.state.bank);
  let a = await E.auditionPatch(rq, info, bass, { gEng: 20 });
  const d3 = E.parse[C.DUMP](await rq(E.req.dump()), info);
  const st3 = [];
  for (let i = 0; i < 16; i++) st3.push(E.parse[C.STEP_GET](await rq(E.req.stepGet(i))));
  const soundOk = (d, pt) => d.p.every((v, i) => (KEPT.includes(i) ? v === d2b.p[i] : v === pt.p[i]));
  ok(!a.wrote && d3.engine === bass.engine && soundOk(d3, bass) && st3.every((s) => !s.n) && js(m.state.bank) === flashBefore
    && d3.p[0] === 77 && d3.p[25] === 5 && d3.p[29] === 12 && d3.p[49] === 2,
    "librarian: audition = G_ENGSEL + SETs of the sound; no pattern, mix, LEN or key; no flash write");
  /* notes in the sequencer stay too */
  await rq(E.req.stepSet(20, { n: 1, notes: [50, 0, 0, 0], time: 0, flags: 0, vel: 90 }));
  const bell = await E.bank.get(rq, info, 1);
  a = await E.auditionPatch(rq, info, { ...bell, pattern: [[72, 0], [74, 0]] }, { gEng: 20 });
  const s0 = E.parse[C.STEP_GET](await rq(E.req.stepGet(0))), s20 = E.parse[C.STEP_GET](await rq(E.req.stepGet(20)));
  const d4 = E.parse[C.DUMP](await rq(E.req.dump()), info);
  ok(!a.wrote && !s0.n && s20.notes[0] === 50 && d4.engine === 1 && soundOk(d4, bell), "librarian: audition keeps the sequencer as it is");

  /* ties as the firmware keeps them: no note; a rest has no flags */
  const tied = [[60, 1], [61, 4], [0, 4], [0, 3], [64, 2]];
  const norm = E.patternFromSteps(E.stepsFromPattern(tied));
  rc = await E.bank.put(rq, 12, { ...cap, name: "TIES", pattern: tied });
  const g3 = await E.bank.get(rq, info, 12);
  ok(rc === 0 && js(norm.slice(0, 5)) === js([[60, 1], [0, 4], [0, 4], [0, 0], [64, 2]]) && js(g3.pattern) === js(norm),
    "librarian: pattern ties / rests normalised like the firmware");
  rc = await E.bank.store(rq, 13, "");
  ok(rc === 0 && (await E.bank.get(rq, info, 13)).name === `${info.engines[d4.engine]} 14`, "librarian: UP_STORE with no name -> automatic name");

  /* library files */
  const ctx = { keys, engines: info.engines, firmware: info.version, pe0: info.pe0 };
  const pts = [cap, { ...bass, engineName: info.engines[bass.engine], tags: ["bass", "device"] }];
  const file = JSON.parse(JSON.stringify(E.libraryFile("library", pts, ctx)));
  ok(file.format === "felucca-library" && file.version === 1 && file.pCount === 71 && file.paramLabels.length === 71 && file.engines.length === 12,
    "library file: versioned, with P_COUNT, labels and engines");
  const back = E.readLibraryFile(file, ctx);
  ok(back.patches.length === 2 && !back.skipped && eq(back.patches[0].p, cap.p) && eq(back.patches[1].p, bass.p)
    && js(back.patches[1].pattern) === js(bass.pattern) && back.patches[1].tags.join() === "bass,device" && back.patches[1].engineName === "PHASE",
    "library file: write -> read round trip");
  /* a future firmware: one more parameter at id 5, engines in another order and one of them gone */
  const keys2 = [...keys.slice(0, 5), "NEW", ...keys.slice(5)];
  const eng2 = ["PHASE", "ANALOG", "SAMPLE"];
  const fut = E.readLibraryFile(file, { keys: keys2, engines: eng2 });
  const p0 = fut.patches[0].p;
  ok(fut.patches.length === 2 && p0.length === 72 && p0[5] === null && p0[6] === cap.p[5] && p0[71] === cap.p[70]
    && fut.patches[0].engine === 1 && fut.patches[1].engine === 0, "library file: other ids / engine order mapped by label and name");
  /* a library written by 2.5 (61 parameters, the engine's from 53) read by 2.6 (71, from 63): the engine's values
   * land on the new ids, the 2.6 parameters stay unset (their defaults) */
  const keysOld = [...keys.slice(0, 53), ...keys.slice(63)];
  const oldFile = JSON.parse(JSON.stringify(E.libraryFile("old", [{ ...cap, p: [...cap.p.slice(0, 53), ...cap.p.slice(63)] }],
    { keys: keysOld, engines: info.engines, firmware: "SLOOP 2.5", pe0: 53 })));
  const up = E.readLibraryFile(oldFile, ctx), pu = up.patches[0].p;
  ok(oldFile.pCount === 61 && pu.length === 71 && pu.slice(63).join() === cap.p.slice(63).join() && pu[52] === cap.p[52]
    && pu.slice(53, 63).every((x) => x == null), "library file: a 2.5 one (61 parameters) read by 2.6: E1..E8 at 63..70, the new ones unset");
  const lost = E.readLibraryFile({ ...file, patches: [{ ...file.patches[0], engineName: "WAVETABLE" }] }, ctx);
  ok(lost.patches.length === 0 && lost.skipped === 1, "library file: a patch for an unknown engine is skipped");
  const bankFile = E.libraryFile("bank", [{ ...g, engineName: "ANALOG", slot: 10 }], ctx);
  ok(bankFile.kind === "bank" && bankFile.patches[0].slot === 10 && E.readLibraryFile(bankFile, ctx).patches[0].slot === 10, "library file: bank export keeps slot numbers");
  const old = E.readLibraryFile({ format: "felucca-patch", version: 1, engine: 0, preset: 4, engineName: "ANALOG", presetName: "ACID", p: d2.p, steps: E.stepsFromPattern(cap.pattern) }, ctx);
  ok(old.patches.length === 1 && eq(old.patches[0].p, d2.p) && js(old.patches[0].pattern) === js(cap.pattern), "library file: reads the old \"Save to file\" format");
  /* a "Save to file" of 2.5 (61 values, the engine's from 53, no pCount) on 2.6: the engine's 8 at 63..70 */
  const p61 = [...d2.p.slice(0, 53), ...d2.p.slice(63)];
  const o61 = E.readLibraryFile({ format: "felucca-patch", version: 1, engine: 0, preset: 4, engineName: "ANALOG", p: p61 }, ctx).patches[0].p;
  const p71 = E.readLibraryFile({ format: "felucca-patch", version: 1, engine: 0, preset: 4, engineName: "ANALOG", p: d2.p, pCount: 71, pE0: 63 },
    { ...ctx, keys: keys.slice(0, 53).concat(keys.slice(63)) }).patches[0].p;
  ok(o61.length === 71 && o61.slice(63).join() === d2.p.slice(63).join() && o61.slice(53, 63).every((x) => x == null) && o61[52] === d2.p[52]
    && p71.length === 61 && p71.slice(53).join() === d2.p.slice(63).join(),
    "library file: a \"Save to file\" of another version is read by count (the engine's 8 in their place, 61 <-> 71)");
  let threw = false;
  try { E.readLibraryFile({ format: "something" }, ctx); } catch (e) { threw = true; }
  ok(threw, "library file: unknown format -> error");
  done();
}

async function editorLive() {
  const C = E.CMD;
  const { m, link, rq, sent, ev, done } = attachMock({ watchMs: 250 });
  const info = E.parse[C.INFO](await rq(E.req.info()));
  ok(await E.startWatch(rq), "live: WATCH on");

  /* a push between a request and its reply */
  const pend = rq(E.req.dump());
  const kn = m.sim.knob(9, 4);
  const dump = E.parse[C.DUMP](await pend, info);
  const ch = ev.pushes.find((f) => f.cmd === C.CHANGED);
  const cv = ch && E.parse[C.CHANGED](ch.a);
  ok(dump.p.length === 71 && ch && ch.pending === C.DUMP && cv.scope === 0 && cv.id === 9 && cv.value === kn.value && !ev.unknown.length,
    "live: CHANGED while DUMP waits -> push handler, reply still matched");
  const rl = m.sim.reload();
  m.sim.step(3);
  const pend2 = rq(E.req.stepGet(7));
  const s7 = E.parse[C.STEP_GET](await pend2);
  await sleep(10);
  const r = ev.pushes.find((f) => f.cmd === C.RELOAD), sc = ev.pushes.find((f) => f.cmd === C.STEP_CHANGED);
  ok(s7.index === 7 && r && E.parse[C.RELOAD](r.a).preset === rl.preset && sc && E.parse[C.STEP_CHANGED](sc.a).index === 3,
    "live: RELOAD and STEP_CHANGED routed");
  const nr = ev.pushes.filter((f) => f.cmd === C.RELOAD).length;
  await rq(E.req.preset(1, 2));
  await rq(E.req.stepSet(9, { n: 1, notes: [62, 0, 0, 0], time: 0, flags: 0, vel: 90 }));
  await sleep(10);
  ok(ev.pushes.filter((f) => f.cmd === C.RELOAD).length === nr + 1 && !ev.pushes.some((f) => f.cmd === C.STEP_CHANGED && f.a[0] === 9),
    "live: RELOAD after an editor PRESET too, nothing after its STEP_SET");

  /* PING keeps the watch on; without requests it ends */
  for (let i = 0; i < 4; i++) { await sleep(120); await rq(E.req.ping()); }
  let n0 = ev.pushes.length;
  m.sim.knob();
  await sleep(10);
  ok(ev.pushes.length === n0 + 1, "live: PING keeps WATCH on");
  await sleep(320);
  n0 = ev.pushes.length;
  m.sim.knob();
  await sleep(10);
  ok(ev.pushes.length === n0, "live: WATCH ends by itself without requests");

  /* a slider drag: 40 values at once -> one SET in flight + one coalesced, the last value wins */
  const before = sent[C.SET] || 0;
  const all = [];
  for (let v = 0; v < 40; v++) all.push(rq(E.req.set(0, 9, v * 3), { key: "0:9" }));
  await Promise.all(all);
  ok((sent[C.SET] || 0) - before === 2 && m.state.p[9] === 117 && link.idle, "live: drag SETs coalesce (2 frames for 40 values, latest kept)");
  ok(ev.timeouts === 0, "live: no timeouts");
  done();

  /* older firmware: no reply to WATCH -> false without a "no reply" message; no bank */
  const o = attachMock({ legacy: true });
  E.parse[C.INFO](await o.rq(E.req.info()));
  const w = await o.rq(E.req.watch(1), { timeout: 60, retries: 0, quiet: true }).then(() => true, () => false);
  const sw = await E.startWatch(o.rq);
  const bl = await E.bank.list((rr, oo) => o.rq(rr, { ...oo, timeout: 60, quiet: true })).then(() => "listed", (e) => e.message);
  ok(!w && !sw && /^timeout/.test(bl) && o.ev.timeouts === 0, "live: older firmware -> WATCH unanswered (fall back to polling), no bank");
  o.done();
}

/* ------------------------------------------------------ editor protocol v3: tracks --- */
async function editorTracks() {
  const C = E.CMD;
  const { m, rq, ev, done } = attachMock({ watchMs: 1000 });
  const info = E.parse[C.INFO](await rq(E.req.info()));
  const tr = E.parse[C.TRACK](await rq(E.req.track()));
  ok(info.ntrk === 4 && tr.sel === 0 && tr.ntrk === 4 && tr.tracks[0].engine === 0 && tr.tracks[1].engine === 1
    && tr.tracks[3].engine === info.nengines, "tracks: INFO NTRK, TRACK lists 4 (track 4 = drums, engine NENGINES)");
  /* the v1 commands follow the selected track */
  const d0 = E.parse[C.DUMP](await rq(E.req.dump()), info);
  const t1 = E.parse[C.TRACK](await rq(E.req.track(1)));
  const d1 = E.parse[C.DUMP](await rq(E.req.dump()), info);
  await rq(E.req.set(0, 1, 77));
  const td0 = E.parse[C.TRACK_DUMP](await rq(E.req.trackDump(0)), info);
  const td1 = E.parse[C.TRACK_DUMP](await rq(E.req.trackDump(1)), info);
  ok(t1.sel === 1 && d1.engine === 1 && d0.engine === 0 && td1.p[1] === 77 && td0.p[1] === d0.p[1] && td0.p[1] !== 77,
    "tracks: TRACK selects; DUMP / SET act on it, TRACK_DUMP reads any track");
  /* steps of a track that is not selected */
  const w = E.parse[C.TRACK_STEP](await rq(E.req.trackStep(2, 5, { n: 2, notes: [60, 67, 0, 0], time: 0, flags: 1, vel: 99 })));
  const g2 = E.parse[C.TRACK_STEP](await rq(E.req.trackStep(2, 5)));
  const s1 = E.parse[C.STEP_GET](await rq(E.req.stepGet(5)));
  ok(w.track === 2 && g2.n === 2 && g2.notes[1] === 67 && g2.vel === 99 && s1.n === 0, "tracks: TRACK_STEP set / get on another track");
  /* level / mute; the drum level is GLO > DRUMS LEVEL */
  const mx = E.parse[C.TRACK_MIX](await rq(E.req.trackMix(0, 90, 1)));
  const mxd = E.parse[C.TRACK_MIX](await rq(E.req.trackMix(3, 64, 0)));
  const mx2 = E.parse[C.TRACK_MIX](await rq(E.req.trackMix(0)));
  const tr2 = E.parse[C.TRACK](await rq(E.req.track()));
  ok(mx.level === 90 && mx.mute === 1 && mx2.level === 90 && mxd.level === 64 && m.state.g[25] === 64
    && tr2.tracks[0].level === 90 && tr2.tracks[0].mute === 1, "tracks: TRACK_MIX level / mute (drums: G_DRLVL)");
  /* the drum track: no sound to store or load */
  await rq(E.req.track(3));
  const dd = E.parse[C.DUMP](await rq(E.req.dump()), info);
  const us = E.parse[C.UP_STORE](await rq(E.req.upStore(20, "X"), { timeout: 2500, retries: 0 }));
  const ul = E.parse[C.UP_LOAD](await rq(E.req.upLoad(1), { timeout: 2500, retries: 0 }));
  ok(dd.engine === info.nengines && us.rc === 1 && ul.rc === 1, "tracks: drum track selected -> DUMP engine NENGINES, UP_STORE / UP_LOAD rc 1");
  /* pushes carry the selected track */
  await rq(E.req.track(0));
  ok(await E.startWatch(rq), "tracks: WATCH on");
  m.sim.track(2);
  m.sim.step(4);
  await sleep(10);
  const rl = ev.pushes.find((f) => f.cmd === C.RELOAD), sc = ev.pushes.find((f) => f.cmd === C.STEP_CHANGED);
  ok(rl && E.parse[C.RELOAD](rl.a).track === 2 && sc && E.parse[C.STEP_CHANGED](sc.a).track === 2 && E.parse[C.STEP_CHANGED](sc.a).index === 4,
    "tracks: RELOAD / STEP_CHANGED carry the selected track");
  /* projects keep all four tracks */
  await rq(E.req.project(1, 3), { timeout: 4000, retries: 0 });
  await rq(E.req.trackStep(2, 5, { n: 0, notes: [0, 0, 0, 0], time: 2, flags: 0, vel: 0 }));
  await rq(E.req.track(0));
  await rq(E.req.project(0, 3), { timeout: 4000, retries: 0 });
  const back = E.parse[C.TRACK_STEP](await rq(E.req.trackStep(2, 5)));
  const sel = E.parse[C.TRACK](await rq(E.req.track())).sel;
  ok(back.n === 2 && back.notes[0] === 60 && sel === 2, "tracks: PROJECT save / load keeps every track and the selection");
  /* older firmware: no NTRK in INFO, no RELOAD track byte */
  const o = attachMock({ legacy: true });
  const oi = E.parse[C.INFO](await o.rq(E.req.info()));
  ok(oi.ntrk === 0 && E.parse[C.RELOAD]([0, 4]).track === 0 && E.parse[C.STEP_CHANGED]([7]).track === 0, "tracks: older firmware parses (no tracks)");
  o.done();
  done();
}

/* ------------------------------- editor 2.6: SET coalescing per control (SetGate) --- */
/* a user report: DIV 1/8 on the drum track, then 1/8 on a synth track: the second SET never left (the coalescing kept the
   last value sent per control, whatever the track). userSet as editor.html has it, on the mock */
async function editorSetGate() {
  const C = E.CMD;
  const { link, rq, sent, done } = attachMock({});
  const info = E.parse[C.INFO](await rq(E.req.info()));
  let div = -1;
  for (let i = 0; i < info.pcount && div < 0; i++) if (E.parse[C.DESC](await rq(E.req.desc(0, i))).label === "DIV") div = i;
  const gate = new E.SetGate(), k = "0:" + div;
  const pending = (key) => link.hasQueued(key) || (!!link.cur && link.cur.key === key);
  const user = (v) => {                               /* editor.html userSet / sendSet */
    if (!gate.user(k, v, pending(k), Date.now())) return null;
    gate.get(k).sent = v;
    return rq(E.req.set(0, div, v), { key: k });
  };
  const divOf = async (t) => E.parse[C.TRACK_DUMP](await rq(E.req.trackDump(t)), info).p[div];
  await rq(E.req.track(3));
  const d0 = await divOf(0), want = d0 === 1 ? 2 : 1;
  await user(want);
  await rq(E.req.track(0));                           /* (selectTrack -> afterSoundChange: gate.clear()) */
  const n0 = sent[C.SET] || 0;
  const p = user(want);                               /* the same value, track 1: sent, even without the clear */
  if (p) await p;
  ok(div >= 0 && (await divOf(3)) === want && (await divOf(0)) === want && sent[C.SET] === n0 + 1,
    "SetGate: DIV set on the drum track, then the same DIV on track 1: sent, both set");
  gate.clear();
  ok(gate.get(k) === undefined, "SetGate: clear() forgets every control (another track / sound / FM-1)");
  /* a drag: while a SET is pending, the same value is not queued again; a new one replaces the queued one */
  const a = user(5), b = user(5), c = user(6), d = user(6);
  ok(a && b === null && c && d === null, "SetGate: pending: the same value skipped, a new value sent (queued)");
  await Promise.all([a, c]);
  ok((await divOf(0)) === 6, "SetGate: the last value wins");
  ok(user(6) !== null, "SetGate: answered: the same value sent again (a knob may have moved it on the FM-1)");
  await rq(E.req.set(0, div, d0));
  done();
}

/* ------------------------------------------------ editor v3: the mixer (Tracks tab) --- */
async function editorMixer() {
  const C = E.CMD;
  const { m, rq, ev, done } = attachMock({ watchMs: 1000 });
  const info = E.parse[C.INFO](await rq(E.req.info()));
  const PAN = 39, MUTE = 40;
  const m0 = await E.mixer.read(rq, info, { pan: PAN });
  ok(m0.ntrk === 4 && m0.tracks.length === 4 && m0.tracks[1].pan === -24 && m0.tracks[2].pan === 20 && m0.tracks[3].engine === info.nengines
    && m0.tracks.every((x) => Number.isInteger(x.level) && (x.mute === 0 || x.mute === 1)), "mixer: read = TRACK + pan of every track (TRACK_DUMP)");
  /* level / mute of a track that is not selected, and of the drum track (G_DRLVL) */
  const a = await E.mixer.setMix(rq, 2, 70, 1);
  const b = await E.mixer.setMix(rq, 3, 200, 0);
  const m1 = await E.mixer.read(rq, info, { pan: PAN });
  const td2 = E.parse[C.TRACK_DUMP](await rq(E.req.trackDump(2)), info);
  ok(a.level === 70 && a.mute === 1 && b.level === 127 && m.state.g[25] === 127 && m1.tracks[2].level === 70 && m1.tracks[2].mute === 1
    && td2.p[0] === 70 && td2.p[MUTE] === 1 && m1.sel === 0, "mixer: TRACK_MIX level / mute round trip (drums: G_DRLVL, clamped)");
  /* pan of another track: selected for the SET, the selection put back, no RELOAD pushed */
  ok(await E.startWatch(rq), "mixer: WATCH on");
  const pushes = ev.pushes.length;
  /* (the v3 path: firmware 0.8 has no TRACK_PARAM) */
  const p2 = await E.mixer.setPan(rq, 2, 0, PAN, -40);
  const p0 = await E.mixer.setPan(rq, 0, 0, PAN, 99);
  await sleep(10);
  const m2 = await E.mixer.read(rq, info, { pan: PAN });
  const d0 = E.parse[C.DUMP](await rq(E.req.dump()), info);
  ok(p2 === -40 && p0 === 63 && m2.sel === 0 && m2.tracks[2].pan === -40 && m2.tracks[0].pan === 63 && d0.p[PAN] === 63 && m2.tracks[1].pan === -24
    && ev.pushes.length === pushes, "mixer: pan of any track via SET (other track selected for a moment, then back; no push)");
  /* the device's TRACKS page: level of the selected track pushes CHANGED; REC arm shows in TRACK */
  m.sim.level(33);
  m.sim.arm(1);
  await sleep(10);
  const ch = ev.pushes.filter((f) => f.cmd === C.CHANGED).map((f) => E.parse[C.CHANGED](f.a)).pop();
  const m3 = await E.mixer.read(rq, info, { pan: PAN });
  ok(ch && ch.scope === 0 && ch.id === 0 && ch.value === 33 && m3.tracks[0].level === 33 && m3.tracks[1].armed === 1 && m3.tracks[0].armed === 0,
    "mixer: device-side level (CHANGED push) and REC arm read back");
  await rq(E.req.track(3));
  m.sim.level(90);
  await sleep(10);
  const chd = E.parse[C.CHANGED](ev.pushes.filter((f) => f.cmd === C.CHANGED).pop().a);
  ok(chd.scope === 1 && chd.id === 25 && chd.value === 90, "mixer: drum level on the device pushes G_DRLVL");
  /* the drum track's steps: GM notes, shown and typed by name */
  const st = [];
  for (let i = 0; i < 16; i++) st.push(E.parse[C.TRACK_STEP](await rq(E.req.trackStep(3, i))));
  const names = st[0].notes.slice(0, st[0].n).map(E.drumName).join(" ");
  ok(names === "KICK CHH" && st[4].notes.slice(0, 2).map(E.drumName).join(" ") === "SNARE CHH" && E.drumName(20) === "20",
    "mixer: the mock drum track holds a GM pattern (KICK CHH ...)");
  ok(JSON.stringify(E.parseNotes("kick CHH 49")) === "[36,42,49]" && JSON.stringify(E.parseNotes("C4 SNARE")) === "[60,38]" && E.parseNotes("KICKS") === null
    && Object.keys(E.GM_DRUM).length === 47 && new Set(Object.values(E.GM_DRUM)).size === 47, "mixer: GM drum names parse (unique, 35..81)");
  done();
}

/* ------------------------------------- editor v4: TRACK_PARAM and TRACK_CHANGED --- */
async function editorTrackParam() {
  const C = E.CMD, PAN = 39, MUTE = 40;
  const { m, rq, sent, ev, done } = attachMock({ watchMs: 1000 });
  const info = E.parse[C.INFO](await rq(E.req.info()));
  const w1 = E.parse[C.WATCH](await rq(E.req.watch(1)));
  m.sim.param(2, PAN, 11);
  await sleep(10);
  ok(w1.on === 1 && !ev.pushes.some((f) => f.cmd === C.TRACK_CHANGED), "v4: WATCH 1 answers 1 as before (no TRACK_CHANGED pushes)");
  ok(await E.startWatch(rq) === 3, "v4: WATCH 3 -> 3 (TRACK_PARAM / TRACK_CHANGED known)");
  const tracks0 = sent[C.TRACK] || 0, pushes = ev.pushes.length;
  const g = E.parse[C.TRACK_PARAM](await rq(E.req.trackParam(1, PAN)));
  const p2 = await E.mixer.setPan(rq, 2, 0, PAN, -40, true);
  const p1 = await E.mixer.setPan(rq, 1, 0, PAN, 99, true);
  const alg = E.parse[C.TRACK_PARAM](await rq(E.req.trackParam(1, info.pe0, 50)));   /* track 2 is DIGITAL: ALG 0..7 */
  const lv = E.parse[C.TRACK_PARAM](await rq(E.req.trackParam(3, 0, -5)));
  const sel = E.parse[C.TRACK](await rq(E.req.track()));
  const td2 = E.parse[C.TRACK_DUMP](await rq(E.req.trackDump(2)), info);
  await sleep(10);
  ok(g.track === 1 && g.id === PAN && g.value === -24 && p2 === -40 && p1 === 63 && alg.value === 7 && lv.value === 0 && td2.p[PAN] === -40
    && sel.sel === 0 && (sent[C.TRACK] || 0) === tracks0 + 1 && ev.pushes.length === pushes,
    "v4: TRACK_PARAM get / set on other tracks (clamped as SET, selection kept, no push)");
  const bad = await rq(E.req.trackParam(4, PAN), { timeout: 60, retries: 0, quiet: true }).then(() => "reply", () => "none");
  ok(bad === "none", "v4: TRACK_PARAM of track 5: no reply");
  /* device-side changes: CHANGED for the selected track, TRACK_CHANGED for the others */
  m.sim.param(2, PAN, 30);
  m.sim.param(3, MUTE, 1);
  m.sim.param(0, PAN, -7);
  await sleep(10);
  const tc = ev.pushes.filter((f) => f.cmd === C.TRACK_CHANGED).map((f) => E.parse[C.TRACK_CHANGED](f.a));
  const ch = ev.pushes.filter((f) => f.cmd === C.CHANGED).map((f) => E.parse[C.CHANGED](f.a)).pop();
  ok(tc.length === 2 && tc[0].track === 2 && tc[0].id === PAN && tc[0].value === 30 && tc[1].track === 3 && tc[1].id === MUTE && tc[1].value === 1
    && ch && ch.scope === 0 && ch.id === PAN && ch.value === -7 && !ev.unknown.length, "v4: TRACK_CHANGED pushes for the other tracks, CHANGED for the selected one");
  done();
  /* firmware 0.8 (v3): WATCH 3 answers 1, TRACK_PARAM unanswered: the editor keeps the select / restore path */
  const o = attachMock({ v3: true, watchMs: 1000 });
  E.parse[C.INFO](await o.rq(E.req.info()));
  const on = await E.startWatch(o.rq);
  const tp = await o.rq(E.req.trackParam(1, PAN), { timeout: 60, retries: 0, quiet: true }).then(() => "reply", () => "none");
  const pv = await E.mixer.setPan(o.rq, 2, 0, PAN, 5, false);
  ok(on === 1 && tp === "none" && pv === 5 && o.ev.timeouts === 0 && !o.ev.unknown.length, "v4: v3 firmware -> WATCH 1, no TRACK_PARAM (pan by select / restore)");
  o.done();
}

/* ------------------------------------- editor v9 (SLOOP 2.4): FM6 patches --- */
async function editorV9() {
  const C = E.CMD, F = E.FM6;
  /* the editor's factory patches == the firmware's (build/gen/felucca_fm6.h, tools/gen_fm6_patches.py) */
  const gp = join(HERE, "../build/gen/felucca_fm6.h");
  if (existsSync(gp)) {
    const h = readFileSync(gp, "utf8").replace(/\/\*[^*]*\*\//g, "");
    const arr = (name) => (new RegExp(`${name}\\[[^\\]]*\\](?:\\[[^\\]]*\\])?\\s*=\\s*\\{([\\s\\S]*?)\\};`).exec(h) || [])[1] || "";
    const nums = (x) => (x.match(/\d+/g) || []).map(Number);
    const fac = nums(arr("FM6_FACTORY"));
    ok(js(nums(arr("FM6_INIT"))) === js(Array.from(F.INIT_PK)) && fac.length === F.FACTORY_PK.length * 128
      && F.FACTORY_PK.every((pk, i) => js(Array.from(pk)) === js(fac.slice(i * 128, i * 128 + 128))),
    "v9: the editor's init and factory patches == felucca_fm6.h");
  }
  /* pack / unpack: every voice survives, every value into its range */
  let rt = true;
  for (let n = 0; n < 50; n++) {
    const v = F.sanitize(Uint8Array.from({ length: F.SIZE }, (_, i) => (i * 37 + n * 101 + (i * n) % 13) & 127));
    if (js(Array.from(F.unpack(F.pack(v)))) !== js(Array.from(v))) rt = false;
  }
  ok(rt, "v9: FM6 pack / unpack round trip (50 voices, sanitized)");
  /* SysEx: one voice (163 bytes) and a 32-voice bank (4104 bytes), names, checksums */
  const v1 = F.factory(0), one = F.singleSysex(v1), p1 = F.parseSysex(one);
  ok(one.length === 163 && one[0] === 0xF0 && one[162] === 0xF7 && p1.voices.length === 1 && !p1.badSum
    && js(Array.from(p1.voices[0].v)) === js(Array.from(v1)), "v9: single-voice SysEx: 163 bytes, read back the same");
  const voices = Array.from({ length: 32 }, (_, k) => { const v = F.factory(k % F.FACTORY_PK.length); F.setName(v, `VOICE ${k + 1}`); return v; });
  const bank = F.bankSysex(voices), pb = F.parseSysex(bank);
  ok(bank.length === 4104 && pb.voices.length === 32 && !pb.badSum && pb.voices[31].name.trim() === "VOICE 32"
    && pb.voices.every((x, k) => js(Array.from(x.v)) === js(Array.from(F.unpack(F.pack(voices[k]))))),
  "v9: 32-voice bank SysEx: 4104 bytes, 32 voices with their names");
  const badb = Uint8Array.from(bank); badb[4102] ^= 1;
  ok(F.parseSysex(badb).badSum, "v9: a bank with a wrong checksum is read but flagged");
  /* damaged and unusual files (2.4, after Felucca 1.0.3) */
  const cut = bank.subarray(0, 6 + 128 * 10 + 50);               /* cut inside voice 11 */
  const pc = F.parseSysex(cut);
  ok(pc.voices.length === 10 && pc.short === 1 && pc.voices[9].name.trim() === "VOICE 10", "v9: a bank cut short: its whole voices, flagged");
  const two = Uint8Array.from([...bank, ...one]), p2 = F.parseSysex(two);
  ok(p2.voices.length === 33 && !p2.badSum && !p2.short, "v9: a bank and a voice in one file: 33 voices");
  const nof7 = Uint8Array.from([...bank.subarray(0, 4102), ...one]);   /* the bank with no checksum / F7 */
  ok(F.parseSysex(nof7).voices.length === 33, "v9: a message with no end: the next one still read");
  const raw2 = new Uint8Array(8192); raw2.set(bank.subarray(6, 4102), 0); raw2.set(bank.subarray(6, 4102), 4096);
  ok(F.parseSysex(raw2).voices.length === 64, "v9: raw data, two 4096-byte banks: 64 voices");
  const fm4 = Uint8Array.from([0xF0, 0x43, 0x00, 0x04, 0x20, 0x00, ...new Array(4096).fill(0), 0, 0xF7]), p4 = F.parseSysex(fm4);
  ok(!p4.voices.length && p4.kinds.includes("fm4") && p4.sysex, "v9: 4-operator voices: none read, said so");
  const roland = F.parseSysex(Uint8Array.from([0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41, 0xF7]));
  ok(!roland.voices.length && roland.kinds[0] === "maker:41", "v9: another maker's SysEx: its ID");
  ok(!F.parseSysex(Uint8Array.from([1, 2, 3])).sysex, "v9: not SysEx at all: said so");
  /* the device: FM6_LIST / GET / PUT / ERASE */
  const { m, rq, done } = attachMock({});
  const info = E.parse[C.INFO](await rq(E.req.info()));
  ok(info.proto === 10 && info.nengines === 12 && info.engines[9] === "FM6", "v9: INFO ends with 10 (v10 includes v9), FM6 is engine 9");
  const L = E.parse[C.FM6_LIST](await rq(E.req.fm6List()));
  if (!(L.factory === 8 && L.bank === 27 && L.slots.length === 35)) console.log("  FM6_LIST", L.factory, L.bank, L.slots.length, js(L.slots.slice(0, 10)));
  ok(L.factory === 8 && L.bank === 27 && L.slots.length === 35 && L.slots[0].used, "v9: FM6_LIST: 8 factory patches, 27 bank slots");
  const mine = F.factory(3); F.setName(mine, "MY BELL");
  let r = E.parse[C.FM6_PUT](await rq(E.req.fm6Put(F.TARGET.TRACK, 1, F.pack(mine))));
  const g = E.parse[C.FM6_GET](await rq(E.req.fm6Get(F.TARGET.TRACK, 1)));
  ok(r.rc === 0 && g.rc === 0 && F.name(F.unpack(g.packed)).trim() === "MY BELL", "v9: FM6_PUT to track 2, FM6_GET reads it back");
  r = E.parse[C.FM6_PUT](await rq(E.req.fm6Put(F.TARGET.BANK, 4, F.pack(mine)), { timeout: 3000 }));
  const L2 = E.parse[C.FM6_LIST](await rq(E.req.fm6List()));
  ok(r.rc === 0 && L2.slots[8 + 4].used && L2.slots[8 + 4].name.trim() === "MY BELL", "v9: stored in bank slot B5, listed by name");
  m.sim.play(true);
  r = E.parse[C.FM6_PUT](await rq(E.req.fm6Put(F.TARGET.BANK, 5, F.pack(mine)), { timeout: 3000 }));
  const re = E.parse[C.FM6_ERASE](await rq(E.req.fm6Erase(4), { timeout: 3000 }));
  ok(r.rc === 3 && re.rc === 3, "v9: bank writes refused while the song plays (rc 3)");
  m.sim.play(false);
  const e1 = E.parse[C.FM6_ERASE](await rq(E.req.fm6Erase(4), { timeout: 3000 }));
  const ge = E.parse[C.FM6_GET](await rq(E.req.fm6Get(F.TARGET.BANK, 4)));
  const bad = E.parse[C.FM6_GET](await rq(E.req.fm6Get(F.TARGET.FACTORY, 30)));
  ok(e1.rc === 0 && ge.rc === 2 && bad.rc === 1, "v9: FM6_ERASE empties the slot (GET rc 2), a bad index is rc 1");
  done();
  const old = attachMock({ v8: true });
  const oi = E.parse[C.INFO](await old.rq(E.req.info()));
  let none = false;
  try { await old.rq(E.req.fm6List(), { timeout: 200, retries: 0 }); } catch (e) { none = true; }
  ok(oi.proto === 8 && none, "v9: a v8 device says 8 and does not answer FM6_LIST (the editor hides the FM6 panel)");
  old.done();
}

/* ------------------------------------- editor v6: backup / restore --- */
async function editorBackup() {
  const C = E.CMD;
  const { rq, done } = attachMock({});
  const info = E.parse[C.INFO](await rq(E.req.info()));
  ok(info.proto >= 6, "backup: INFO protocol v6 or later");
  const ec = readFileSync(join(HERE, "../firmware/src/editor.c"), "utf8");
  ok(/ED_BK_IDS\[\] = \{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 32, 33, 34, 35\}/.test(ec) && E.backupObjects && [10].every((id) => ec.includes("id == 10u")),
    "backup: the object ids == editor.c ED_BK_IDS (35: USR4, 9: SYN kits, 10: MIDI LEARN)");
  await rq(E.req.upStore(3, "BACKUP ME"));
  await rq(E.req.project(1, 2), { timeout: 4000, retries: 0 });
  const s = Int16Array.from({ length: 3000 }, (_, i) => Math.round(8000 * Math.sin(i / 7)));
  const { hdr, data } = E.buildSlot("test", [{ s, root: 60 }]);
  await rq(E.req.smpBegin(1), { timeout: 1000, retries: 0 });
  for (let off = 0; off < data.length; off += 256) await rq(E.req.smpWrite(1, E.SMP.DATA_OFF + off, data.subarray(off, off + 256)), { timeout: 1000 });
  await rq(E.req.smpEnd(1, hdr), { timeout: 2000, retries: 0 });
  const A = await E.backupCapture(rq, info);
  const obj = (id) => A.objects.find((o) => o.id === id);
  ok(A.format === "sloop-backup" && A.objects.map((o) => o.id).join() === "0,1,2,3,4,5,6,7,8,9,32,33,34,35" && obj(9).len === 1464 && obj(33).len > 512 && obj(35).len === 0
    && obj(4).len > 0 && obj(3).len === 0 && obj(32).len === 0, "backup: LIST + GET: 14 objects (project in C, sample in USR2, B empty, the FM6 bank, the SYN kits, USR4)");
  await rq(E.req.upErase(3));
  await rq(E.req.smpErase(1), { timeout: 2500, retries: 0 });
  await rq(E.req.set(0, 3, 5));
  await rq(E.req.project(1, 1), { timeout: 4000, retries: 0 });
  await E.backupRestore(rq, JSON.parse(JSON.stringify(A)));   /* as written to disk and read back */
  const B = await E.backupCapture(rq, info);
  ok(js(B.objects) === js(A.objects), "backup: restore -> the same bytes again (presets, projects, samples, settings)");
  const bad = JSON.parse(JSON.stringify(A));
  bad.objects[2].data = E.b64enc(E.b64dec(bad.objects[2].data).map((x, i) => i === 3 ? x ^ 1 : x));
  let caught = "";
  try { E.backupObjects(bad); } catch (e) { caught = e.code; }
  ok(caught === "bkBad", "backup: a damaged file is refused before anything is written");
  done();
  const old = attachMock({ noBackup: true });
  const oi = E.parse[C.INFO](await old.rq(E.req.info()));
  ok(oi.proto === 5, "backup: firmware without it says protocol 5 (the editor hides backup)");
  old.done();
}

/* ------------------------------------- editor v5 (SLOOP 2.0): lanes, levels, ratchets --- */
async function editorV5() {
  const C = E.CMD;
  const { m, rq, ev, done } = attachMock({ watchMs: 1000 });
  const info = E.parse[C.INFO](await rq(E.req.info()));
  ok(info.proto === 10 && /SLOOP/.test(info.version) && info.pcount === 71 && info.gcount === 45 && info.pe0 === 63, "v5..v10: INFO ends with the protocol version (10: SYN kits); 45 globals (2.5: drum DLY; 2.6: GLO > MIDI, TEXTURE, BEND)");
  /* the firmware says the same: ED_DRUM_STEP is command 33, INFO sends 5, P_CHORD / the master globals as the mock has them */
  const ec = readFileSync(join(HERE, "../firmware/src/editor.c"), "utf8"), pc = readFileSync(join(HERE, "../firmware/src/params.c"), "utf8");
  const en = (/enum \{ ED_INFO = 1,([^}]*)\}/.exec(ec) || [])[1] || "";
  const names = ["ED_INFO", ...en.replace(/\/\*[^*]*\*\//g, "").split(",").map((x) => x.trim()).filter(Boolean)];
  ok(names.indexOf("ED_DRUM_STEP") + 1 === C.DRUM_STEP && names.indexOf("ED_TRACK_CHANGED") + 1 === C.TRACK_CHANGED
    && names.indexOf("ED_BK_LIST") + 1 === C.BK_LIST && names.indexOf("ED_BK_PUT") + 1 === C.BK_PUT
    && names.indexOf("ED_LOCK_GET") + 1 === C.LOCK_GET && names.indexOf("ED_LOCK_SET") + 1 === C.LOCK_SET
    && names.indexOf("ED_MICRO_GET") + 1 === C.MICRO_GET && names.indexOf("ED_MICRO_SET") + 1 === C.MICRO_SET
    && names.indexOf("ED_FILL_GET") + 1 === C.FILL_GET && names.indexOf("ED_FILL_SET") + 1 === C.FILL_SET
    && /ED_FM6_GET = 68, ED_FM6_PUT, ED_FM6_LIST, ED_FM6_ERASE/.test(ec) && C.FM6_GET === 68 && C.FM6_ERASE === 71
    && /#define ED_PROTO 10u/.test(ec) && /ed_b\(ED_PROTO\);/.test(ec)
    && /enum \{ ED_DSYN_LIST = 72, ED_DSYN_GET, ED_DSYN_PUT, ED_DSYN_STORE, ED_DSYN_PLAY \};/.test(readFileSync(join(HERE, "../firmware/src/editor_dsyn.c"), "utf8"))
    && C.DSYN_LIST === 72 && C.DSYN_PLAY === 76, "v5..v10: command numbers and INFO == editor.c / editor_dsyn.c");
  const enumNames = (id) => (new RegExp(`${id}\\[\\] = \\{([^}]*)\\}`).exec(pc) || [])[1].split(",").map((x) => x.trim().replace(/"/g, ""));
  const chord = E.parse[C.DESC](await rq(E.req.desc(0, 49)));
  const gd = [];
  for (let i = 27; i < 32; i++) gd.push(E.parse[C.DESC](await rq(E.req.desc(1, i))));
  ok(chord.label === "CHORD" && chord.names.join() === enumNames("N_CHORD").join() && /\[P_CHORD\] = PE\("CHORD", N_CHORD, 0\)/.test(pc)
    && gd.map((d) => d.label).join() === "DUST,DUCK,FILT,ROLL,NEW" && gd[3].names.join() === enumNames("N_ROLL").join() && gd[3].def === 1
    && /\[G_DUST\] = PD\("DUST", F_PCT, 0, 127, 0\)/.test(pc) && /\[G_FILT\] = PD\("FILT", F_FILT, -64, 63, 0\)/.test(pc)
    && /\[G_ROLL\] = PE\("ROLL", N_ROLL, 1\)/.test(pc) && /\[G_NEWPRJ\] = PE\("NEW", N_GO, 0\)/.test(pc),
    "v5: CHORD, DUST, DUCK, FILT, ROLL, NEW (mock == params.c)");
  /* the value formats (params.c fmt_value) */
  const fv = (fmt, v, max = 127, min = 0) => E.fmtValue({ fmt, min, max, names: [] }, v).join("");
  ok(fv(E.F.SWING, 0, 100) === "0" && fv(E.F.SWING, 50, 100) === "50" && fv(E.F.SWING, 100, 100) === "100"
    && fv(E.F.FILT, 0, 63, -64) === "OFF" && fv(E.F.FILT, -64, 63, -64) === "LP100%" && fv(E.F.FILT, -32, 63, -64) === "LP50%" && fv(E.F.FILT, 63, 63, -64) === "HP100%"
    && fv(E.F.PCT, 127) === "100%" && fv(E.F.PCT, 64) === "50%" && fv(E.F.PCT, 60, 120) === "50%", "v5: SWING 0..100 (2.5), FILT LP / OFF / HP, PCT of the range");
  /* a synth step keeps its levels and ratchets; an old-style write clears them (as the firmware) */
  const w = E.parse[C.STEP_SET](await rq(E.req.stepSet(3, { n: 2, notes: [60, 67, 0, 0], time: 0, flags: 0, vel: 100, lvl: 0b11000110, rat: 0b10000011 })));
  const g = E.parse[C.STEP_GET](await rq(E.req.stepGet(3)));
  const w2 = E.parse[C.STEP_SET](await rq(E.req.stepSet(3, { n: 1, notes: [62, 0, 0, 0], time: 0, flags: 0, vel: 90 })));
  ok(w.lvl === 0b11000110 && g.lvl === 0b11000110 && g.rat === 0b10000011 && w2.lvl === 0 && w2.rat === 0 && E.req.stepSet(1, { n: 0, notes: [], time: 2, flags: 0, vel: 0 })[1].length === 9,
    "v5: STEP_SET / STEP_GET level and ratchet bytes (8 bits each), none for an old step");
  /* the drum track: 16 lanes with their level and ratchet; lane 15 uses the top bits */
  const d = E.emptyDrum();
  d.on = (1 << 0) | (1 << 4) | (1 << 15); d.lvl[0] = 3; d.lvl[4] = 1; d.rat[4] = 3; d.lvl[15] = 2; d.rat[15] = 1;
  const ds = E.parse[C.DRUM_STEP](await rq(E.req.drumStep(5, d)));
  const dg = E.parse[C.DRUM_STEP](await rq(E.req.drumStep(5)));
  ok(ds.index === 5 && dg.on === d.on && dg.lvl.join() === d.lvl.join() && dg.rat.join() === d.rat.join()
    && E.req.drumStep(5, d)[1].every((b) => b >= 0 && b < 128) && E.req.drumStep(5, d)[1].length === 14, "v5: DRUM_STEP round trip (16 lanes, levels, ratchets)");
  /* old commands see the lanes as GM notes (the first four, ACC when one is hard); an old write lands on lanes */
  const old = E.parse[C.TRACK_STEP](await rq(E.req.trackStep(3, 5)));
  await rq(E.req.trackStep(3, 6, { n: 2, notes: [38, 46, 0, 0], time: 0, flags: 1, vel: 100 }));
  const d6 = E.parse[C.DRUM_STEP](await rq(E.req.drumStep(6)));
  ok(old.n === 3 && old.notes.slice(0, 3).join() === "36,42,56" && old.flags === 1 && d6.on === ((1 << 2) | (1 << 5)) && d6.lvl[2] === 3 && d6.lvl[5] === 3,
    "v5: the drum track through TRACK_STEP: lanes <-> GM notes");
  ok(E.DRUM_LANES.length === 16 && E.DRUM_LANES[0][0] === 36 && E.DRUM_LANES[15][0] === 56 && E.LV_NAMES.join() === "NORM,GHOST,SOFT,HARD",
    "v5: the 16 lanes (kick .. cowbell), 4 levels");
  const dc = readFileSync(join(HERE, "../firmware/src/drums.c"), "utf8");
  const lanes = ((/LANE_NOTE\[DRUM_LANES\] = \{([^}]*)\}/.exec(dc) || [])[1] || "").split(",").map((x) => +x);
  ok(lanes.join() === E.DRUM_LANES.map((x) => x[0]).join(), "v5: lane notes == drums.c LANE_NOTE");
  /* the kit: the drum track's P_E0 */
  await rq(E.req.track(3));
  const kit = E.parse[C.DESC](await rq(E.req.desc(0, info.pe0)));
  const dk = (/DRUM_KIT_NAMES\[\] = \{([^}]*)\}/.exec(dc) || [])[1] || "";
  const gen = join(HERE, "../build/gen/felucca_drumkits.h");
  const dsList = existsSync(gen) ? ((/#define DS_KIT_NAME_LIST (.*)/.exec(readFileSync(gen, "utf8")) || [])[1] || "") : null;
  const fwKits = dsList == null ? null : dk.replace("DS_KIT_NAME_LIST", dsList).split(",").map((x) => x.trim().replace(/"/g, ""));
  ok(kit.label === "KIT" && kit.names[5] === "808" && kit.names.length === kit.max + 1 && (!fwKits || fwKits.join() === kit.names.join()),
    `v5: the drum track's KIT (${kit.names.length} kits${fwKits ? ", == drums.c" : ""})`);
  /* TRACK ends with the solo mask */
  m.state.solo = 0b0101;
  const tr = E.parse[C.TRACK](await rq(E.req.track()));
  ok(tr.solo === 5 && tr.sel === 3 && tr.tracks.length === 4, "v5: TRACK reports the soloed tracks");
  ok(!ev.unknown.length, "v5: no unmatched replies");
  done();
  /* firmware 0.8 (v3): no DRUM_STEP, no protocol byte, steps without the extra bytes */
  const o = attachMock({ v3: true });
  const oi = E.parse[C.INFO](await o.rq(E.req.info()));
  const os = E.parse[C.STEP_GET](await o.rq(E.req.stepGet(0)));
  const nd = await o.rq(E.req.drumStep(0), { timeout: 60, retries: 0, quiet: true }).then(() => "reply", () => "none");
  ok(oi.proto === 0 && os.lvl === undefined && nd === "none", "v5: older firmware -> proto 0, no DRUM_STEP (the editor keeps GM notes)");
  o.done();
}

/* ----------------------------- editor protocol v7: nudges and parameter locks (SLOOP 2.4) --- */
async function editorV7() {
  const C = E.CMD;
  const { m, rq, ev, done } = attachMock({ watchMs: 1000 });
  const info = E.parse[C.INFO](await rq(E.req.info()));
  ok(info.proto === 10, "v7: INFO ends with 10 (v10 includes v7)");
  /* the lockable set == seq.c p_lockable (the ids as core.h names them) */
  const sc = readFileSync(join(HERE, "../firmware/src/seq.c"), "utf8"), ch = readFileSync(join(HERE, "../firmware/src/core.h"), "utf8");
  const ids = {}; let n = 0;
  for (const nm of (/enum \{\s*\/\* per-track parameters \*\/([^}]*)\}/.exec(ch) || [])[1].replace(/\/\*[\s\S]*?\*\//g, "").split(",").map((x) => x.trim()).filter(Boolean)) ids[nm] = n++;
  const lockableC = (id) => id <= ids.P_LD_AMP || id === ids.P_SGATE || (id >= ids.P_DIST && id <= ids.P_REV) || id === ids.P_GLIDE || id === ids.P_PAN
    || id === ids.P_DETUNE || (id >= ids.P_SLCR && id <= ids.P_SLDEPTH) || (id >= ids.P_E0 && id <= ids.P_E7) || id === ids.P_TFLT
    || (id >= ids.P_IA && id <= ids.P_IMIX);   /* 2.6: the INSERT's values */
  let same = ids.P_COUNT === 71 && ids.P_E0 === 63;
  for (let id = 0; id < 71; id++) same = same && !!E.lockable(id, 63) === !!lockableC(id);
  for (let id = 0; id < 61; id++) same = same && !!E.lockable(id, 53) === (id >= 53 || !!lockableC(id));   /* (older firmware: E 53..60) */
  ok(same && /id <= P_LD_AMP \|\| id == P_SGATE \|\| \(id >= P_DIST && id <= P_REV\) \|\| id == P_GLIDE \|\| id == P_PAN/.test(sc)
    && !E.lockable(ids.P_SLEN) && !E.lockable(ids.P_AMODE) && !E.lockable(ids.P_VOICE) && !E.lockable(ids.P_CHORD) && E.lockable(ids.P_E7),
    "v7: the lockable parameters (editor == seq.c p_lockable; LEN, ARP, VOICE, CHORD are not)");
  /* MICRO: 64 bytes offset by 64; a set clamps to -32..31 */
  const mg = E.parse[C.MICRO_GET](await rq(E.req.microGet(0)));
  ok(mg.track === 0 && mg.micro.length === 64 && mg.micro.every((x) => x === 0), "v7: MICRO_GET: 64 nudges, all 0");
  const ms = E.parse[C.MICRO_SET](await rq(E.req.microSet(0, 4, -8)));
  const ms2 = E.parse[C.MICRO_SET](await rq(E.req.microSet(0, 5, 100)));
  const ms3 = E.parse[C.MICRO_SET](await rq(E.req.microSet(0, 6, -100)));
  const mg2 = E.parse[C.MICRO_GET](await rq(E.req.microGet(0)));
  ok(ms.step === 4 && ms.micro === -8 && ms2.micro === 31 && ms3.micro === -32 && mg2.micro[4] === -8 && mg2.micro[5] === 31 && mg2.micro[6] === -32
    && E.req.microSet(0, 4, -8)[1].join() === "0,4,56" && E.req.microSet(0, 4, 31)[1][2] === 95 && E.req.microSet(0, 4, -32)[1][2] === 32,
    "v7: MICRO_SET (sent + 64: -8 -> 56, 31 -> 95, -32 -> 32), clamped to -32..31");
  /* LOCK: set, move, list, delete; refused on a non-lockable parameter, bounded */
  const l1 = E.parse[C.LOCK_SET](await rq(E.req.lockSet(0, 2, ids.P_E0, 1)));
  const l2 = E.parse[C.LOCK_SET](await rq(E.req.lockSet(0, 2, ids.P_DIST, 500)));
  const l3 = E.parse[C.LOCK_SET](await rq(E.req.lockSet(0, 2, ids.P_SLEN, 8)));
  const l4 = E.parse[C.LOCK_SET](await rq(E.req.lockSet(0, 70, ids.P_E0, 1)));
  const lg = E.parse[C.LOCK_GET](await rq(E.req.lockGet(0)));
  ok(l1.rc === 0 && l1.has === 1 && l1.value === 1 && l2.rc === 0 && l2.value === 127 && l3.rc === 2 && !l3.has && l3.value === null && l4.rc === 1
    && lg.track === 0 && lg.locks.length === 2 && lg.locks[0].step === 2 && lg.locks[0].param === ids.P_E0 && lg.locks[1].value === 127
    && E.req.lockSet(0, 2, 5, -30)[1].length === 5 && E.req.lockSet(0, 2, 5)[1].length === 3,
    "v7: LOCK_SET makes and clamps a lock (DIST 500 -> 127), refuses LEN (rc 2) and step 70 (rc 1); LOCK_GET lists them");
  const l5 = E.parse[C.LOCK_SET](await rq(E.req.lockSet(0, 2, ids.P_E0, 3)));
  const l6 = E.parse[C.LOCK_SET](await rq(E.req.lockSet(0, 2, ids.P_E0)));
  const lg2 = E.parse[C.LOCK_GET](await rq(E.req.lockGet(0)));
  ok(l5.value === 3 && l6.rc === 0 && l6.has === 0 && lg2.locks.length === 1 && lg2.locks[0].param === ids.P_DIST,
    "v7: LOCK_SET again moves the lock; without a value it deletes it");
  for (let i = 0; i < 23; i++) await rq(E.req.lockSet(0, i, ids.P_LEVEL, 100));   /* 24 in all */
  const full = E.parse[C.LOCK_SET](await rq(E.req.lockSet(0, 40, ids.P_LEVEL, 100)));
  ok(full.rc === 3 && E.parse[C.LOCK_GET](await rq(E.req.lockGet(0))).locks.length === 24, "v7: the 25th lock is refused (rc 3, 24 a track)");
  /* another track is untouched; a device-side nudge pushes STEP_CHANGED */
  const lg3 = E.parse[C.LOCK_GET](await rq(E.req.lockGet(1)));
  await rq(E.req.watch(1));
  m.sim.nudge(7, 12);
  await sleep(30);
  const sc7 = ev.pushes.find((f) => f.cmd === C.STEP_CHANGED && f.a[0] === 7);
  ok(lg3.locks.length === 0 && !!sc7 && E.parse[C.MICRO_GET](await rq(E.req.microGet(0))).micro[7] === 12,
    "v7: other tracks untouched; a nudge on the device -> STEP_CHANGED of its step");
  ok(!ev.unknown.length, "v7: no unmatched replies");
  done();
  /* a v6 device (SLOOP 2.3): INFO 6, no reply to the v7 commands */
  const o = attachMock({ noBackup: true });
  const oi = E.parse[C.INFO](await o.rq(E.req.info()));
  const nl = await o.rq(E.req.lockGet(0), { timeout: 60, retries: 0, quiet: true }).then(() => "reply", () => "none");
  ok(oi.proto === 5 && nl === "none", "v7: a device without the commands: no reply (the editor hides the step detail)");
  o.done();
}

/* ----------------------------- editor protocol v8: fill conditions (SLOOP 2.4) --- */
async function editorV8() {
  const C = E.CMD;
  const { m, rq, ev, done } = attachMock({ watchMs: 1000 });
  const info = E.parse[C.INFO](await rq(E.req.info()));
  ok(info.proto === 10, "v8: INFO ends with 10 (v10 includes v8)");
  /* the condition codes == core.h FC_*; the bit layout of a 2-bit step field */
  const ch = readFileSync(join(HERE, "../firmware/src/core.h"), "utf8"), sc = readFileSync(join(HERE, "../firmware/src/seq.c"), "utf8");
  ok(/enum \{ FC_NORM, FC_FILL, FC_NOFILL \}/.test(ch) && /uint8_t fill\[NSTEP \/ 4\];/.test(ch)
    && /t->fill\[idx \/ 4u\] >> \(2u \* \(idx % 4u\)\)\) & 3u/.test(sc) && E.FC.FILL === 1 && E.FC.NOFILL === 2,
    "v8: FC codes and the 2-bit layout == core.h / seq.c step_fill");
  const g0 = E.parse[C.FILL_GET](await rq(E.req.fillGet(0)));
  ok(g0.track === 0 && g0.fill.length === 16 && g0.fill.every((x) => x === 0), "v8: FILL_GET: 16 bytes, every step normal");
  const s1 = E.parse[C.FILL_SET](await rq(E.req.fillSet(0, 4, E.FC.FILL)));
  const s2 = E.parse[C.FILL_SET](await rq(E.req.fillSet(0, 3, E.FC.NOFILL)));   /* (bits 6..7 of byte 0: the top bit crosses pack7) */
  const s3 = E.parse[C.FILL_SET](await rq(E.req.fillSet(0, 63, 3)));            /* 3: normal */
  const g1 = E.parse[C.FILL_GET](await rq(E.req.fillGet(0)));
  ok(s1.step === 4 && s1.cond === 1 && s2.cond === 2 && s3.cond === 0 && g1.fill[1] === 1 && g1.fill[0] === 0x80 && g1.fill[15] === 0
    && E.fillGet(g1.fill, 4) === 1 && E.fillGet(g1.fill, 3) === 2 && E.fillGet(g1.fill, 5) === 0
    && E.req.fillSet(0, 4, 1)[1].join() === "0,4,1" && E.req.fillGet(2)[1].join() === "2",
    "v8: FILL_SET sets a step (3 -> normal); FILL_GET returns the bytes as stored (0x80 through pack7)");
  {
    const b = new Uint8Array(16); E.fillSet(b, 7, 2); E.fillSet(b, 0, 1); E.fillSet(b, 7, 0);
    const rt = E.unpack7(E.pack7(Uint8Array.from([0x80, 0xFF, 0x01, 0x7F, 0x00, 0xC3, 0x55, 0xAA, 0x01, 0x80, 0, 0, 0, 0, 0, 0x40])));
    ok(E.fillGet(b, 0) === 1 && E.fillGet(b, 7) === 0 && b[1] === 0 && rt.length === 16 && rt[0] === 0x80 && rt[1] === 0xFF && rt[7] === 0xAA && rt[15] === 0x40,
      "v8: fillSet / fillGet on the client; pack7 keeps the top bits of 16 bytes (3 groups)");
  }
  /* another track untouched; a device-side change (SEQ + step + OCT+) pushes STEP_CHANGED */
  const g2 = E.parse[C.FILL_GET](await rq(E.req.fillGet(1)));
  await rq(E.req.watch(1));
  m.sim.fill(9, 2);
  await sleep(30);
  const sc9 = ev.pushes.find((f) => f.cmd === C.STEP_CHANGED && f.a[0] === 9);
  ok(g2.fill.every((x) => x === 0) && !!sc9 && E.fillGet(E.parse[C.FILL_GET](await rq(E.req.fillGet(0))).fill, 9) === 2,
    "v8: other tracks untouched; a condition set on the device -> STEP_CHANGED of its step");
  ok(!ev.unknown.length, "v8: no unmatched replies");
  done();
  /* a v7 device: INFO 7, no reply to the v8 commands */
  const o = attachMock({ v7: true });
  const oi = E.parse[C.INFO](await o.rq(E.req.info()));
  const nf = await o.rq(E.req.fillGet(0), { timeout: 60, retries: 0, quiet: true }).then(() => "reply", () => "none");
  const ml = await o.rq(E.req.microGet(0), { timeout: 200, retries: 0, quiet: true }).then(() => "reply", () => "none");
  ok(oi.proto === 7 && nf === "none" && ml === "reply", "v8: a v7 device: INFO 7, locks yes, no reply to FILL_GET (the editor hides the condition)");
  o.done();
}

/* ------------------------------------------------- 2.4: the drum kit page --- */
await (async () => {
  const lanes = { "BD Kick 808.wav": 0, "Snare_01.wav": 2, "HH closed.wav": 4, "OH open.wav": 5, "Crash cymbal.wav": 11, "Clap.wav": 3,
    "Low Tom.wav": 9, "Hi Tom.wav": 10, "rimshot.aif": 7, "Cowbell 1.wav": 15, "shaker.wav": 13, "Conga hi.wav": 14, "ride.wav": 12,
    "pedal hat.wav": 6, "piano C4.wav": -1, "kick2.wav": 1, "Snare 2.wav": 8 };
  const got = Object.entries(lanes).filter(([n, l]) => E.kitLaneOf(n) !== l).map(([n]) => `${n}->${E.kitLaneOf(n)}`);
  ok(!got.length, "drum kit: file names sorted onto the lanes" + (got.length ? ` (${got.join(", ")})` : ""));
  const s = (n, a = 20000) => Int16Array.from({ length: n }, (_, i) => Math.round(a * Math.sin(i / 7)));
  const pads = Array(16).fill(null);
  pads[0] = { s: s(8000), fname: "kick.wav", tune: 0, gain: 0, len: 0 };
  ok(E.kitPlace(pads, "BD second kick.wav") === 1 && E.kitPlace(pads, "piano.wav") === 1 && E.kitPlace(pads, "x.wav", 9) === 9,
    "drum kit: a taken lane gives way (KICK -> KICK 2), unknown names the first free pad, a drop its pad");
  pads[2] = { s: s(6000), fname: "snare.wav", tune: 12, gain: -6, len: 0 };
  pads[5] = { s: s(20000), fname: "oh.wav", tune: 0, gain: 0, len: 4410 };
  const p2 = E.kitPad(pads[2]), p5 = E.kitPad(pads[5]);
  const pk = (x) => x.reduce((a, v) => Math.max(a, Math.abs(v)), 0);
  ok(Math.abs(p2.length - 3000) <= 2 && Math.abs(pk(p2) / 20000 - 0.501) < 0.03 && E.kitLen(pads[2]) - p2.length <= 1,
    "drum kit: +12 st plays it twice as fast (half the length), -6 dB half the level");
  ok(p5.length === 4410 && Math.abs(p5[4409]) < 400 && pk(p5.subarray(0, 4000)) > 15000, "drum kit: LENGTH cuts it, with a 4 ms fade (no click)");
  const zs = E.kitZones(pads), b = E.buildSlot("KIT", zs);
  const hz = (j) => { const o = 32 + j * 28; return [b.hdr[o + 25], b.hdr[o + 26], new DataView(b.hdr.buffer).getInt16(o + 20, true) / 16]; };
  ok(zs.length === 3 && b.hdr[6] === 3 && [0, 1, 2].every((j) => { const [lo, hi, root] = hz(j); return lo === hi && hi === root; })
    && [hz(0)[0], hz(1)[0], hz(2)[0]].join() === "36,38,46",
    "drum kit: one zone per pad, lo = hi = root = the lane's note (drums.c LANE_NOTE: KICK 36, SNARE 38, OPEN HAT 46)");
  const big = Array(16).fill(null).map((_, l) => ({ s: s(30000), fname: l + ".wav", tune: 0, gain: 0, len: 0 }));
  ok(E.kitBytes(big) > 81408 && E.kitFit(big, 81408 - 64) && E.kitBytes(big) <= 81408 - 64 && E.kitBytes(big) > 81408 - 64 - 64,
    "drum kit: FIT shortens the longest pads to fill the slot, no more");
  {   /* KIT USR3+4: a big kit shared out over two slots, each pad whole in one */
    const R = 81408 - 64, mk = (secs) => ({ s: s(Math.round(secs * 22050)), fname: "x.wav", tune: 0, gain: 0, len: 0 });
    const kit = Array(16).fill(null);
    [0.6, 0.3, 0.3, 0.15, 2.5, 1.0, 0.2, 0.2, 0.3, 0.6, 0.6, 3.0, 2.5, 0.3, 0.4, 0.3].forEach((x, l) => { kit[l] = mk(x); });
    const tot = E.kitBytes(kit), sp = E.kitSplit(kit, R);
    const load = (ls) => ls.reduce((a, l) => a + ((E.kitLen(kit[l]) + 1) >> 1), 0);
    ok(tot > R && tot <= 2 * R && sp && sp[0].length + sp[1].length === 16 && load(sp[0]) <= R && load(sp[1]) <= R
      && new Set([...sp[0], ...sp[1]]).size === 16,
      `drum kit: USR3+4 takes a ${(tot * 2 / 22050).toFixed(1)} s kit no slot could, every pad whole in one of the two`);
    const huge = Array(16).fill(null).map(() => mk(1.5));
    ok(!E.kitSplit(huge, R) && E.kitFit(huge, R, 2) && E.kitSplit(huge, R) && E.kitBytes(huge) > 2 * R * 0.9,
      "drum kit: ... FIT for the pair fills both slots (no more than they hold)");
    const one = Array(16).fill(null); one[0] = mk(9);
    ok(!E.kitSplit(one, R) && E.kitFit(one, R, 2) && E.kitSplit(one, R) && E.kitSplit(one, R)[1].length === 0,
      "drum kit: ... a sound longer than a slot is cut to one slot");
  }
  const files = [{ name: "KIT/01 KICK.wav", data: new Uint8Array([1, 2, 3]) }, { name: "KIT/03 SNARE.wav", data: new Uint8Array(1000).fill(7) }];
  const back = await E.zipRead(E.zipStore(files));
  ok(back.length === 2 && back[0].name === "KIT/01 KICK.wav" && back[1].data.length === 1000 && back[1].data[999] === 7, "drum kit: a kit ZIP reads back (zipRead)");
  const dfl = await (async () => {                     /* a deflated entry, as zip tools write them */
    const raw = new Uint8Array(await new Response(new Blob([new Uint8Array(500).fill(65)]).stream().pipeThrough(new CompressionStream("deflate-raw"))).arrayBuffer());
    const z = E.zipStore([{ name: "a.wav", data: raw }]);
    z[8] = 8; z[raw.length + 30 + 5 + 10] = 8;          /* method 8 in the local header and in the directory */
    return (await E.zipRead(z))[0];
  })();
  ok(dfl.data.length === 500 && dfl.data[499] === 65, "drum kit: ... deflated ZIP entries too");
})();

/* ------------------------------ SLOOP 2.5: length, piano roll, MIDI files, song --- */
async function editor25() {
  const { ROLL, SMF, ARR } = E, R = ROLL.rest, len = 16;
  const apply = (st, m) => { const o = st.map((x) => ({ ...x })); for (const [i, v] of m) o[i] = v; return o; };
  const shape = (st) => st.slice(0, len).map((x) => (x.time === 0 && x.n ? "N" + x.notes.slice(0, x.n).join("+") : x.time === 1 ? "-" : ".")).join(" ");
  /* the piano roll */
  let st = Array.from({ length: 64 }, R);
  ok(ROLL.steps(1 / 4, 2) === 4 && ROLL.steps(1, 2) === 16 && ROLL.steps(1 / 16, 7) === 1 && ROLL.steps(1 / 2, 4) === 6 && ROLL.steps(1 / 8, 8) === 1,
    "roll: a note length in steps (1/4 at 1/16 = 4, 1BAR = 1, at least 1, triplets)");
  st = apply(st, ROLL.place(st, len, 0, 60, 4));
  st = apply(st, ROLL.place(st, len, 0, 64, 1));            /* same step: into the chord, its length kept */
  st = apply(st, ROLL.place(st, len, 8, 67, 2));
  ok(shape(st) === "N60+64 - - - . . . . N67 - . . . . . ." && ROLL.chords(st, len).map((c) => `${c.at}:${c.dur}`).join() === "0:4,8:2",
    "roll: place a note, a chord on its first step, a second note");
  const full = apply(st, new Map([[0, { ...st[0], n: 4, notes: [60, 64, 67, 71] }]]));
  ok(ROLL.place(full, len, 0, 72, 1).size === 0 && ROLL.place(st, len, 0, 60, 1).size === 0, "roll: a fifth note, or the same note twice: nothing changes");
  let s2 = apply(st, ROLL.place(st, len, 2, 50, 4));         /* inside the held chord: it ends there */
  ok(shape(s2) === "N60+64 - N50 - - - . . N67 - . . . . . ." && ROLL.owner(s2, 5) === 2 && ROLL.owner(s2, 7) === -1, "roll: a note inside a held chord ends it; owner()");
  s2 = apply(st, ROLL.place(st, len, 6, 48, 6));             /* stops before the next chord */
  ok(shape(s2) === "N60+64 - - - . . N48 - N67 - . . . . . .", "roll: a long note stops before the next chord");
  s2 = apply(st, ROLL.resize(st, len, 0, 7));
  ok(shape(s2) === "N60+64 - - - - - - . N67 - . . . . . ." && shape(apply(st, ROLL.resize(st, len, 0, 12))) === "N60+64 - - - - - - - N67 - . . . . . ."
    && shape(apply(st, ROLL.resize(st, len, 0, 1))) === "N60+64 . . . . . . . N67 - . . . . . ." && shape(apply(st, ROLL.resize(st, len, 8, 99))) === "N60+64 - - - . . . . N67 - - - - - - -",
    "roll: drag a length: longer (ties, up to the next chord or the end), shorter (rests)");
  s2 = apply(st, ROLL.remove(st, len, 0, 64));
  const s3 = apply(s2, ROLL.remove(s2, len, 0, 60));
  ok(shape(s2) === "N60 - - - . . . . N67 - . . . . . ." && shape(s3) === ". . . . . . . . N67 - . . . . . ." && ROLL.remove(st, len, 0, 99).size === 0,
    "roll: remove a chord's note; the last one leaves rests (ties too)");
  /* MIDI files */
  const steps = apply(st, new Map([[8, { ...st[8], flags: 1 }], [0, { ...st[0], vel: 80, lvl: 1 << 2 }]]));
  const notes = SMF.fromSteps(steps, len, 2);
  ok(notes.length === 3 && notes[0].t === 0 && notes[0].dur === 4 * 24 - 1 && notes[0].vel === 80 && notes[1].vel === 42 && notes[2].t === 8 * 24 && notes[2].vel === 127,
    "midi: a track's steps -> notes (ties as lengths; velocity, level, accent as the device plays them)");
  const file = SMF.write(notes, { bpm: 97, ch: 2, name: "SLOOP track 3" });
  const back = SMF.read(file);
  ok(back.format === 0 && back.ppq === 96 && back.bpm === 97 && back.tracks.length === 1 && back.tracks[0].name === "SLOOP track 3"
    && js(back.tracks[0].notes.map((n) => [n.t, n.dur, n.key, n.vel, n.ch])) === js(notes.slice().sort((a, b) => a.t - b.t || a.key - b.key).map((n) => [n.t, n.dur, n.key, n.vel, 2])),
    "midi: write -> read: the same notes, tempo, name, channel");
  const plan = SMF.toSteps(back.tracks[0].notes, back.ppq, 2, 64);
  ok(shape(plan.steps) === shape(steps) && plan.len === 10 && plan.steps[0].vel === 80 && plan.steps[8].vel === 127 && js(Object.values(plan.report)) === js([3, 10, 0, 0, 0, 0, 0]),
    "midi: read back onto the grid: the same chords and lengths, a clean report");
  const py2 = py(`import struct,sys
def vlq(n):
    b=[n&127]; n>>=7
    while n: b.insert(0,128|(n&127)); n>>=7
    return bytes(b)
ev=[(0,b'\\xff\\x51\\x03\\x07\\xa1\\x20')]
def note(t,d,k,v,ch=0): ev.extend([(t,bytes([0x90|ch,k,v])),(t+d,bytes([0x80|ch,k,0]))])
for k in (60,62,64,65,67): note(0,240,k,30+k)          # 5 notes at once: one is cut
note(480,96,72,100); note(520,96,72,90)                 # 520: off the grid (a quarter step = 30) -> the same step, merged
note(960,960,74,100); note(1200,96,76,100)              # cut short by the next chord
note(480*33,96,40,100)                                  # past 64 steps at 1/16
ev.sort(key=lambda e:e[0]); t=0; tr=b''
for at,b in ev: tr+=vlq(at-t)+b; t=at
tr+=b'\\x00\\xff\\x2f\\x00'
hd=b'MThd'+struct.pack('>IHHH',6,1,2,480)
t1=b'MTrk'+struct.pack('>I',4)+b'\\x00\\xff\\x2f\\x00'
sys.stdout.buffer.write(hd+t1+b'MTrk'+struct.pack('>I',len(tr))+tr)`);
  const f2 = SMF.read(new Uint8Array(py2)), p2 = SMF.toSteps(f2.tracks[1].notes, f2.ppq, 2, 64);
  ok(f2.format === 1 && f2.bpm === 120 && f2.tracks.length === 2 && !f2.tracks[0].notes.length && f2.tracks[1].notes.length === 10
    && js(p2.report) === js({ notes: 10, steps: 11, moved: 1, chordCut: 1, overlap: 1, dropped: 1, merged: 1 })
    && p2.steps[0].n === 4 && js(p2.steps[0].notes) === js([62, 64, 65, 67]) && p2.steps[0].vel === 97 && shape(p2.steps).startsWith("N62+64+65+67 - . . N72 . . . N74 - N76 . ."),
    "midi: a format 1 file from Python (480 ppq): 4 notes a step (loudest), merged, moved, cut short, past the end");
  let caught = "";
  try { SMF.read(Uint8Array.from([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14])); } catch (e) { caught = e.message; }
  ok(caught === "not a MIDI file" && (() => { try { SMF.read(file.slice(0, 20)); return true; } catch (e) { return false; } })(), "midi: a non-MIDI file is refused; a cut one reads what is there");
  const ds = Array.from({ length: 64 }, E.emptyDrum);
  ds[0].on = 1 | (1 << 4); ds[0].lvl[4] = 1; ds[4].on = 1 << 2; ds[4].lvl[2] = 3; ds[6].on = 1 << 15; ds[6].lvl[15] = 2;
  const dn = SMF.fromDrums(ds, 16, 2), dp = SMF.toSteps(SMF.read(SMF.write(dn, { ch: 9 })).tracks[0].notes, 96, 2, 64, true);
  ok(dn.map((n) => n.key).join() === "36,42,38,56" && dp.len === 7 && [0, 4, 6].every((i) => dp.dsteps[i].on === ds[i].on && js(dp.dsteps[i].lvl) === js(ds[i].lvl))
    && SMF.laneOf(36) === 0 && SMF.laneOf(35) === 1 && SMF.laneOf(57) === 11 && SMF.laneOf(20) === 0 && SMF.laneOf(90) === 13,
    "midi: the drum track: lanes as GM notes and back, levels ghost..hard kept (drums.c LANE_OF_GM)");
  const dc = readFileSync(join(HERE, "../firmware/src/drums.c"), "utf8");
  const tbl = (/LANE_OF_GM\[81 - 35 \+ 1\] = \{([^}]*)\}/.exec(dc) || [, ""])[1].replace(/\/\*[^*]*\*\//g, "").split(",").map((x) => +x.trim());
  ok(js(tbl) === js(SMF.LANE_OF_GM), "midi: LANE_OF_GM == drums.c");
  /* the song order */
  const sb = E.mockSettings(), song = ARR.decode(sb);
  ok(ARR.ok(sb) && sb.length === 88 && !song.loop && js(song.entries) === js([0, 1, 2, 3].map((scene) => ({ scene, bars: 4 }))), "song: the settings' song order (PER3, arr_defaults)");
  const rows = ARR.group([{ scene: 0, bars: 4 }, { scene: 0, bars: 4 }, { scene: 1, bars: 8 }, { scene: 0, bars: 4 }]);
  ok(js(rows) === js([{ scene: 0, bars: 4, times: 2 }, { scene: 1, bars: 8, times: 1 }, { scene: 0, bars: 4, times: 1 }]) && ARR.expand(rows).length === 4,
    "song: rows (a section n times) <-> entries");
  const enc = ARR.encode(sb, { loop: true, entries: ARR.expand(rows) });
  ok(js(ARR.decode(enc)) === js({ loop: true, entries: ARR.expand(rows) }) && enc.subarray(0, 48).every((v, i) => v === sb[i]) && enc.subarray(84).every((v, i) => v === sb[84 + i]),
    "song: encode keeps the rest of the settings");
  ok(!ARR.valid({ entries: [] }) && !ARR.valid({ entries: Array(17).fill({ scene: 0, bars: 1 }) }) && !ARR.valid({ entries: [{ scene: 4, bars: 1 }] })
    && !ARR.valid({ entries: [{ scene: 0, bars: 65 }] }) && ARR.valid({ entries: Array(16).fill({ scene: 3, bars: 64 }) }), "song: valid == arranger.h arr_valid (1..16 entries, A..D, 1..64 bars)");
  const pc = readFileSync(join(HERE, "../firmware/src/project.c"), "utf8"), ah = readFileSync(join(HERE, "../firmware/src/arranger.h"), "utf8");
  const pt = (/typedef struct \{([^}]*)\} persist_t;/.exec(pc) || [, ""])[1];
  ok(/PERSIST_MAGIC 0x50455233u/.test(pc) && /arr_config_t arrangement;/.test(pt) && /ARR_STEPS 16u/.test(ah)
    && /if \(\(song\.playing \|\| transport_req\) && memcmp\(&p\.arrangement, &arrangement, sizeof arrangement\)\)\s*return 3;/.test(pc),
    "song: PER3, persist_t has the arrangement, 16 steps; a new order refused while playing (project.c)");
  /* read and send it through the backup objects, as the Song page does */
  const { m, rq, done } = attachMock({});
  const get1 = async () => { const L = E.parse[E.CMD.BK_LIST](await rq(E.req.bkList())); const it = L.items.find((x) => x.id === 1);
    return E.parse[E.CMD.BK_GET](await rq(E.req.bkGet(1, 0, it.len))).data; };
  const put1 = async (d) => {
    await rq(E.req.bkBegin(1, d.length, E.crc32(d)));
    await rq(E.req.bkData(1, 0, d));
    return E.parse[E.CMD.BK_PUT](await rq(E.req.bkCommit(1))).rc;
  };
  const cur = await get1();
  const mine = ARR.encode(cur, { loop: true, entries: [{ scene: 2, bars: 8 }, { scene: 3, bars: 2 }] });
  const rc1 = await put1(mine), now = await get1();
  const bad = ARR.encode(cur, { loop: false, entries: [{ scene: 0, bars: 4 }] }); bad[ARR.OFF + 5] = 0;
  const rc2 = await put1(bad);
  m.sim.play(true);
  const rc3 = await put1(ARR.encode(cur, { loop: false, entries: [{ scene: 1, bars: 1 }] }));
  const rc4 = await put1(now);                                 /* the same order while playing: fine */
  m.sim.play(false);
  ok(ARR.ok(cur) && rc1 === 0 && js(ARR.decode(now)) === js({ loop: true, entries: [{ scene: 2, bars: 8 }, { scene: 3, bars: 2 }] }) && rc2 === 2 && rc3 === 3 && rc4 === 0
    && js(ARR.decode(await get1())) === js(ARR.decode(now)), "song: mock: read, send (rc 0), a bad one (rc 2), a new one while playing (rc 3)");
  done();
  /* the page */
  ok(/id="lenval" type="number" min="1" max="64"/.test(html) && /if \(!s && id === pSlen\(\)\) lenChanged\(\);/.test(html) && /if \(!\$\("roll"\)\.hidden\) renderRoll\(\);/.test(html),
    "editor: the LEN control (1..64), the grid and roll redrawn when LEN changes");
}

/* ------------------------------------- SLOOP 2.5: the DRUM SYNTH page (v10) --- */
async function editorDsyn() {
  const { DSYN } = E, C = E.CMD;
  const dsc = readFileSync(join(HERE, "../firmware/src/drum_synth.c"), "utf8");
  const st = (/typedef struct \{([\s\S]*?)\} dsnd_t;/.exec(dsc) || [, ""])[1].replace(/\/\*[\s\S]*?\*\//g, "");
  const fields = [...st.matchAll(/(?:u?int8_t)\s+([^;]+);/g)].flatMap((m) => m[1].split(",").map((x) => x.trim()));
  ok(js(fields) === js(DSYN.FIELDS) && DSYN.SIZE === 22, "dsyn: the 22 fields in dsnd_t's order (drum_synth.c)");
  const ln = ((/DS_LANE_NOTE\[DS_LANES\] = \{([^}]*)\}/.exec(dsc) || [, ""])[1]).split(",").map((x) => +x.trim());
  ok(js(ln) === js(DSYN.LANE_NOTE) && DSYN.LANES_N.length === 16, "dsyn: the lanes' notes == DS_LANE_NOTE");
  ok(/sizeof\(dsu_bank_t\) == 1464u/.test(dsc) && /DSU_MAGIC 0x31555344u/.test(dsc) && DSYN.BANK_SIZE === 1464, "dsyn: backup object 9 == dsu_bank_t (1464 bytes, DSU1)");
  /* the mock's factory kits == the firmware's (when built) */
  const raw = E.b64dec(E.DSYN_MOCK_KITS);
  let hdr = null;
  try { hdr = readFileSync(join(HERE, "../build/gen/felucca_drumkits.h"), "utf8"); } catch (e) { /* (not built) */ }
  if (hdr) {
    const kits = [...hdr.matchAll(/\{"([^"]*)", "[^"]*", (0x[0-9A-Fa-f]+|\d+), \{([\s\S]*?)\}\},/g)];
    const bytes = kits.flatMap((k) => [Number(k[2]), ...[...k[3].matchAll(/\{([^{}]*)\},/g)].flatMap((r) => r[1].split(",").map((x) => +x & 255))]);
    ok(kits.length === 32 && js(kits.map((k) => k[1])) === js(E.DSYN_MOCK_NAMES) && js(Array.from(raw)) === js(bytes),
      "dsyn: the mock's 32 kits == build/gen/felucca_drumkits.h");
  }
  /* viewing never changes a value: decode / encode give every factory sound back */
  let same = true;
  for (let i = 0; i < 32 * 16; i++) {
    const at = Math.floor(i / 16) * 353 + 1 + (i % 16) * 22, b = raw.slice(at, at + 22);
    same = same && js(Array.from(DSYN.encode(DSYN.decode(b)))) === js(Array.from(b));
  }
  const wild = DSYN.decode(DSYN.encode({ wave: 9, noise: 7, clap: 1, pitch: 300, fine: 40, bend: 200, btime: 999, hold: 999, decay: -5,
    tlev: 500, t2: 999, t2lev: 999, click: 999, nlev: 999, nhold: 999, ndec: 999, fmode: 7, fall: 1, res: 99, fcut: 999, fenv: -999, hpf: 999, chip: 999, drive: 999, level: 999 }));
  ok(same && wild.wave === 5 && wild.noise === 4 && wild.clap === 1 && wild.pitch === 127 && wild.fine === 15 && wild.bend === 96 && wild.decay === 0
    && wild.fmode === 3 && wild.res === 31 && wild.fenv === -128 && wild.level === 255 && wild.drive === 127,
    "dsyn: decode / encode: all 512 factory sounds unchanged; out-of-range values into the firmware's ranges");
  ok(Math.round(DSYN.hz(69)) === 440 && Math.round(DSYN.decayMs(127)) === 4000 && Math.round(DSYN.cutHz(0)) === 30 && DSYN.db(132) === 1,
    "dsyn: the units (Hz, ms, cutoff, dB) as tools/gen_tables.py");
  /* files and the backup object */
  const kit = { name: "MY 808", crush: 0x21, src: 0, sounds: Array.from({ length: 16 }, (_, l) => raw.slice(1 + l * 22, 23 + l * 22)) };
  const back = DSYN.fromFile(JSON.parse(JSON.stringify(DSYN.toFile(kit))));
  let bad = "";
  try { DSYN.fromFile({ format: "sloop-drumsynth", sounds: [[1, 2]] }); } catch (e) { bad = e.message; }
  const bank = DSYN.bankEncode([kit, kit, kit, kit]), dec = DSYN.bankDecode(bank);
  ok(back.name === "MY 808" && back.crush === 0x21 && js(back.sounds.map((x) => Array.from(x))) === js(kit.sounds.map((x) => Array.from(x))) && /not a SLOOP/.test(bad)
    && bank.length === 1464 && dec && dec[3].name === "MY 808" && dec[2].crush === 0x21 && js(Array.from(dec[1].sounds[15])) === js(Array.from(kit.sounds[15]))
    && DSYN.bankDecode(bank.slice(1)) === null, "dsyn: a kit file and the backup object, there and back; a wrong file refused");
  /* the device (mock): LIST / GET / PUT / STORE / PLAY */
  const { m, rq, done } = attachMock({});
  let L = E.parse[C.DSYN_LIST](await rq(E.req.dsynList()));
  const g0 = E.parse[C.DSYN_GET](await rq(E.req.dsynGet(DSYN.USER)));
  const f5 = E.parse[C.DSYN_GET](await rq(E.req.dsynGet(5)));
  ok(L.factory === 32 && L.user === 4 && L.stored && L.names[0] === "808" && L.kits.map((k) => k.name).join() === "808,909,TRAP,TECHNO"
    && g0.rc === 0 && g0.name === "808" && g0.src === 0 && js(g0.sounds.map((x) => Array.from(x))) === js(kit.sounds.map((x) => Array.from(x)))
    && f5.rc === 0 && f5.name === "TRAP", "dsyn: LIST (32 + 4, stored), GET SYN1 = the 808, GET a factory kit");
  const o = DSYN.decode(g0.sounds[0]); o.decay = 20; o.wave = 4;
  const p1 = E.parse[C.DSYN_PUT](await rq(E.req.dsynSound(0, 0, DSYN.encode(o))));
  const p2 = E.parse[C.DSYN_PUT](await rq(E.req.dsynHead(0, "BOOM", 0x10, 0)));
  const p3 = E.parse[C.DSYN_PUT](await rq(E.req.dsynCopy(3, 25)));
  const pl = E.parse[C.DSYN_PLAY](await rq(E.req.dsynPlay(0, 0, 100)));
  const g1 = E.parse[C.DSYN_GET](await rq(E.req.dsynGet(DSYN.USER)));
  L = E.parse[C.DSYN_LIST](await rq(E.req.dsynList()));
  ok(p1.rc === 0 && p2.rc === 0 && p3.rc === 0 && pl.rc === 0 && g1.name === "BOOM" && g1.crush === 0x10 && DSYN.decode(g1.sounds[0]).decay === 20
    && DSYN.decode(g1.sounds[0]).wave === 4 && L.kits[3].name === "CHIP" && !L.stored && m.state.dsynPlayed.length === 1 && m.state.dsynPlayed[0].sound[7] === 20,
    "dsyn: PUT a sound, the name and crush, a factory copy; PLAY hears the edit; LIST: not stored");
  m.sim.play(true);
  const s3 = E.parse[C.DSYN_STORE](await rq(E.req.dsynStore()));
  m.sim.play(false);
  const s0 = E.parse[C.DSYN_STORE](await rq(E.req.dsynStore()));
  L = E.parse[C.DSYN_LIST](await rq(E.req.dsynList()));
  const bp = E.parse[C.DSYN_PLAY](await rq(E.req.dsynPlay(0, 16)));
  ok(s3.rc === 3 && s0.rc === 0 && L.stored && bp.rc === 1, "dsyn: STORE while playing: rc 3; stopped: stored; PLAY of no lane: rc 1");
  /* a backup carries the kits, a restore puts them back */
  const info = E.parse[C.INFO](await rq(E.req.info()));
  const A = await E.backupCapture(rq, info);
  await rq(E.req.dsynCopy(0, 1));
  await E.backupRestore(rq, JSON.parse(JSON.stringify(A)));
  const g2 = E.parse[C.DSYN_GET](await rq(E.req.dsynGet(DSYN.USER)));
  ok(A.objects.some((x) => x.id === 9 && x.len === 1464) && g2.name === "BOOM", "dsyn: a backup holds the SYN kits (object 9); a restore puts them back");
  /* 2.6: the MIDI LEARN map, object 10, in a backup only while something is learned */
  const ml = new Uint8Array(40);                            /* (2.6: the map, then GLO > MIDI: 2, 11, 8, drums 10, BEND) */
  ml[0] = 21, ml[1] = (1 << 1) | (34 << 1);                 /* CC 21 -> track 1 (bits 7..8), code 34 (bits 9..15) */
  ml[32] = 2, ml[33] = 11, ml[34] = 8, ml[35] = 10, ml[36] = 2;
  m.state.ml = ml.slice();
  const M = await E.backupCapture(rq, info);
  m.state.ml = new Uint8Array(32);
  const N = await E.backupCapture(rq, info);
  m.state.ml = new Uint8Array(40);
  await E.backupRestore(rq, JSON.parse(JSON.stringify(M)));
  ok(M.objects.some((x) => x.id === 10 && x.len === 40) && !N.objects.some((x) => x.id === 10) && m.state.ml[0] === 21 && m.state.ml[1] === ml[1]
    && m.state.ml[33] === 11, "midi learn: a backup holds the learned CCs and GLO > MIDI (object 10, only when not the default); a restore puts them back");
  {   /* 2.6: a 2.6 backup without object 10 (the defaults then): restored as the defaults, not left as they are now */
    const N1 = JSON.parse(JSON.stringify(N));
    N1.firmware = "SLOOP 2.6";
    await E.backupRestore(rq, N1);
    ok(m.state.ml.length === 40 && m.state.ml.slice(0, 32).every((x) => x === 0) && m.state.ml[32] === 1 && m.state.ml[33] === 2
      && m.state.ml[34] === 3 && m.state.ml[35] === 10 && m.state.ml[36] === 2,
      "backup: a 2.6 one without object 10 resets MIDI LEARN and GLO > MIDI to the defaults (they were then)");
  }
  {   /* 2.6: a backup with projects newer than the device reads: refused before anything is written */
    const f6 = Uint8Array.of(0x36, 0x4E, 0x55, 0x46, 1), f5 = Uint8Array.of(0x35, 0x4E, 0x55, 0x46);
    ok(E.bkProjFormat(f6) === 6 && E.bkProjFormat(f5) === 5 && E.bkProjFormat(Uint8Array.of(1, 2, 3, 4)) === 0
      && E.bkDevFormat({ pcount: 71 }) === 6 && E.bkDevFormat({ pcount: 61 }) === 5 && E.bkDevFormat({ pcount: 53 }) === 4
      && E.bkFwAtLeast("SLOOP 2.6", [2, 5, 1]) && E.bkFwAtLeast("SLOOP 2.6", [2, 5, 1]) && !E.bkFwAtLeast("SLOOP 2.5", [2, 5, 1])
      && !E.bkFwAtLeast("FELUCCA SLOOP 2.0 (MOCK)", [2, 5, 1]),
      "backup: the project format of an object, what a device reads (INFO), the firmware compared");
    let err = null, writes = 0;
    const rq25 = async (r, o) => {                    /* a 2.5 FM-1 (INFO: 61 parameters); anything else counts as a write */
      if (r[0] === C.INFO) return { info25: { ...E.parse[C.INFO](await rq(r, o)), pcount: 61, pe0: 53 } };
      writes++;
      return rq(r, o);
    };
    const B6 = JSON.parse(JSON.stringify(M));
    const p2 = B6.objects.find((x) => x.id === 2);
    if (p2) {
      const d = E.b64dec(p2.data);
      d.set([0x36, 0x4E, 0x55, 0x46], 0);
      p2.data = E.b64enc(d);
      p2.crc = E.crc32(d) >>> 0;
    }
    const parse0 = E.parse[C.INFO];
    E.parse[C.INFO] = (a) => (a && a.info25 ? a.info25 : parse0(a));
    try { await E.backupRestore(rq25, B6); } catch (e) { err = e; }
    E.parse[C.INFO] = parse0;
    ok(!!p2 && err && err.code === "bkNewer" && writes === 0, "backup: 2.6 projects into a 2.5 FM-1: refused (bkNewer), nothing written");
  }
  done();
  const old = attachMock({ v9: true });
  const oi = E.parse[C.INFO](await old.rq(E.req.info()));
  const B = await E.backupCapture(old.rq, oi);
  ok(oi.proto === 9 && !B.objects.some((x) => x.id === 9), "dsyn: a v9 device says 9 (the page says it needs 2.5), no object 9");
  await E.backupRestore(old.rq, JSON.parse(JSON.stringify(A)));    /* a 2.5 backup into a v9 device: object 9 skipped */
  ok(true, "dsyn: a 2.5 backup restores into a v9 device (object 9 skipped)");
  await E.backupRestore(old.rq, JSON.parse(JSON.stringify(M)));    /* a 2.6 backup with CCs learned into an older device */
  ok(true, "midi learn: a 2.6 backup with learned CCs restores into an older device (object 10 skipped)");
  old.done();
}

/* ------------------------------------------- 2.5: a DX7 cartridge into the bank --- */
async function editorCart() {
  const C = E.CMD, F = E.FM6;
  const fc = readFileSync(join(HERE, "../firmware/src/fm6_bank.c"), "utf8");
  const voices = Array.from({ length: 32 }, (_, k) => { const v = F.factory(k % F.FACTORY_PK.length); F.setName(v, `CART ${k + 1}`); return v; });
  const pb = F.parseSysex(F.bankSysex(voices));
  const slots = new Array(27).fill(null); slots[0] = F.pack(voices[0]); slots[26] = F.pack(voices[5]);
  const o = F.bankObj(slots), u = F.bankUnobj(o);
  const bad = Uint8Array.from(o); bad[16 + 3] = 200;
  const wrong = Uint8Array.from(o); wrong[6] = 32;
  ok(/sizeof\(fm6_bank_t\) == 3472u/.test(fc) && /FM6_BANK_MAGIC 0x42364D46u/.test(fc) && o.length === 3472
    && js(Array.from(o.subarray(0, 12))) === js([0x46, 0x4D, 0x36, 0x42, 1, 0, 27, 0, 1, 0, 0, 4])
    && u[0] && u[26] && !u[1] && js(Array.from(u[26])) === js(Array.from(slots[26])) && F.bankUnobj(new Uint8Array(0)).every((x) => x === null)
    && F.bankUnobj(bad) === null && F.bankUnobj(wrong) === null && F.bankUnobj(o.subarray(1)) === null,
  "cart: the bank object == fm6_bank_t (3472 bytes, FM6B, 27 slots); a bad one is refused");
  const { m, rq, sent, done } = attachMock({});
  const mine = F.factory(3); F.setName(mine, "MY BELL");
  await rq(E.req.fm6Put(F.TARGET.BANK, 4, F.pack(mine)), { timeout: 3000 });     /* B5 */
  await rq(E.req.fm6Put(F.TARGET.BANK, 1, F.pack(mine)), { timeout: 3000 });     /* B2 */
  const packs = [0, 1, 2].map((k) => F.pack(pb.voices[k].v));
  m.sim.play(true);
  const r3 = await E.fm6CartWrite(rq, packs);
  let L = E.parse[C.FM6_LIST](await rq(E.req.fm6List()));
  ok(r3 === 3 && L.slots[8 + 1].name.trim() === "MY BELL" && L.slots[8].name.trim() === "MY TINES", "cart: while the song plays: rc 3, the bank unchanged");
  m.sim.play(false);
  const p0 = sent[C.BK_PUT] || 0, r0 = await E.fm6CartWrite(rq, packs), puts = sent[C.BK_PUT] - p0;
  L = E.parse[C.FM6_LIST](await rq(E.req.fm6List()));
  const g = E.parse[C.FM6_GET](await rq(E.req.fm6Get(F.TARGET.BANK, 2)));
  ok(r0 === 0 && L.slots.slice(8, 11).map((x) => x.name.trim()).join() === "CART 1,CART 2,CART 3" && L.slots[8 + 4].name.trim() === "MY BELL"
    && !L.slots[8 + 3].used && js(Array.from(F.unpack(g.packed))) === js(Array.from(pb.voices[2].v)) && puts === 2 + Math.ceil(3472 / 256),
  "cart: 3 voices -> B1-B3 in one write (B2 replaced, B5 kept, B4 still empty)");
  const all = pb.voices.slice(0, 27).map((x) => F.pack(x.v));
  const rAll = await E.fm6CartWrite(rq, all);
  L = E.parse[C.FM6_LIST](await rq(E.req.fm6List()));
  let tooMany = "";
  try { await E.fm6CartWrite(rq, pb.voices.map((x) => F.pack(x.v))); } catch (e) { tooMany = e.message; }
  ok(rAll === 0 && L.slots.slice(8).every((x, k) => x.used && x.name.trim() === `CART ${k + 1}`) && /32 voices/.test(tooMany),
    "cart: the first 27 of a 32-voice cartridge fill B1-B27; 32 at once refused");
  const info = E.parse[C.INFO](await rq(E.req.info()));
  const A = await E.backupCapture(rq, info);
  ok(A.objects.some((x) => x.id === 8 && x.len === 3472), "cart: a backup holds the filled bank (object 8)");
  /* 2.5: also as user presets, named after the voices, each on its bank slot */
  const fake = { total: 32, slots: Array.from({ length: 32 }, (_, s) => ({ slot: s, used: s === 0 || s === 2 ? 1 : 0, name: s ? "X" : "MINE" })) };
  ok(js(E.fm6CartSlots(fake, 3, "free")) === js([1, 3, 4]) && js(E.fm6CartSlots(fake, 3, 29)) === js([29, 30, 31])
    && E.fm6CartSlots(fake, 3, 30) === null && E.fm6CartSlots(fake, 31, "free") === null && E.fm6CartSlots(fake, 30, "free").length === 30,
  "cart: preset slots: the first free ones, or from a slot; null when they do not fit");
  const base = Array.from({ length: info.pcount }, (_, i) => i % 3);
  const pr = E.fm6CartPreset(info, base, 8, 4, "  BRASS 1   "), pr0 = E.fm6CartPreset(info, base, 8, 0, "");
  ok(pr.engine === 9 && pr.name === "BRASS 1" && pr.p.length === 71 && pr.p[info.pe0 + 7] === 12 && pr.p[info.pe0 + 6] === base[info.pe0 + 6]
    && pr.p[0] === base[0] && pr0.name === "FM6 B1" && pr0.p[info.pe0 + 7] === 8, "cart: a preset = FM6, the voice's name, PTCH = its B slot, the rest as given");
  const ul = await E.bank.list(rq);
  const free = E.fm6CartSlots(ul, 3, "free");
  const ups = free.map((s, i) => ({ slot: s, pt: E.fm6CartPreset(info, base, 8, i, pb.voices[i].name) }));
  m.sim.play(true);
  const u3 = await E.fm6CartPresets(rq, ups);
  m.sim.play(false);
  const u0 = await E.fm6CartPresets(rq, ups);
  const ul2 = await E.bank.list(rq), g2 = await E.bank.get(rq, info, free[2]);
  ok(u3 === 3 && u0 === 0 && free.every((s, i) => ul2.slots[s].used && ul2.slots[s].name === `CART ${i + 1}` && ul2.slots[s].engine === 9)
    && g2.engine === 9 && g2.p[info.pe0 + 7] === 10 && ul.slots.filter((x) => x.used).every((x) => ul2.slots[x.slot].name === x.name),
  "cart: user presets CART 1-3 in the free slots (FM6, PTCH B1-B3), the others kept; refused while the song plays");
  done();
  const old = attachMock({ v8: true });
  ok(await old.rq(E.req.info()) && (await E.fm6CartWrite(old.rq, packs)) === "old", "cart: a device before v9: 'old' (the page says it needs 2.4)");
  old.done();
}

/* ------------------------------------------------- editor tabs and strings --- */
function editorTabs() {
  const tabs = [...html.matchAll(/<button role="tab" data-tab="(\w+)"/g)].map((x) => x[1]);
  const panels = [...html.matchAll(/<section class="panel" id="p-(\w+)" data-tab="(\w+)"/g)].map((x) => [x[1], x[2]]);
  const TABS = JSON.parse((/const TABS = (\[[^\]]*\]);/.exec(html) || [])[1] || "[]");
  ok(tabs.length === 10 && js(tabs) === js(TABS) && js(panels.map((x) => x[1])) === js(TABS) && panels.every(([a, b]) => a === b),
    `editor: ${tabs.length} tabs, one panel each (${tabs.join(" ")})`);
  ok(/localStorage\.setItem\(TAB_KEY/.test(html) && /try \{ localStorage/.test(html) && /history\.replaceState\([^)]*"#" \+ name\)/.test(html)
    && /addEventListener\("hashchange"/.test(html), "editor: last tab in localStorage (try/catch) and in the URL hash");
  /* every string key in both languages */
  const tb = html.slice(html.indexOf("const TEXT = {"), html.indexOf("\n};", html.indexOf("const TEXT = {")) + 2);
  const TEXT = vm.runInNewContext(tb.replace("const TEXT =", "(") + ")");
  const ja = new Set(Object.keys(TEXT.ja)), en = new Set(Object.keys(TEXT.en));
  const used = new Set([...html.matchAll(/data-t="(\w+)"|\bt\("(\w+)"\)|sayK\("(\w+)"|hint = "(\w+)"/g)].map((x) => x[1] || x[2] || x[3] || x[4]));
  for (const k of ["needDevice", "smpNone", "bankConnect", "bankNone", "selectedTrack", "selectTrack", "drumHelp", "notesHelp", "live", "polling"]) used.add(k);
  const miss = [...used].filter((k) => !ja.has(k) || !en.has(k));
  const odd = [...ja].filter((k) => !en.has(k)).concat([...en].filter((k) => !ja.has(k)));
  ok(!miss.length && !odd.length, `editor: every string in ja and en (${used.size} used${miss.length ? ", missing " + miss : ""}${odd.length ? ", one language only " + odd : ""})`);
  /* the page script parses (the browser's view of it) */
  const script = html.slice(html.indexOf("<script>") + 8, html.lastIndexOf("</script>"));
  let err = null;
  try { new vm.Script(script); } catch (e) { err = e.message; }
  ok(!err, "editor: page script compiles" + (err ? ` (${err})` : ""));
  {   /* 2.4: the device's palette as tokens (:root), used through var(--...); outside them only #000 (text on a colour) */
    const css = html.slice(html.indexOf("<style>"), html.indexOf("</style>")).replace(/:root\s*\{[^}]*\}/g, "");
    const hex = [...css.matchAll(/#[0-9a-f]{3,6}\b/gi)].map((m) => m[0].toLowerCase()).filter((c) => c !== "#000");
    ok(!hex.length && /--t1: #287cff; --t2: #1ecc70; --t3: #ffc618; --t4: #ff621a;/.test(html),
      "editor: the track colours of the device (ui_studio.c TE_COL), colours only as tokens" + (hex.length ? ` (${hex.join(" ")})` : ""));
    const te = readFileSync(join(HERE, "../firmware/src/ui_studio.c"), "utf8");
    ok(/TE_COL\[4\] = \{RGB\(40, 124, 255\), RGB\(30, 204, 112\), RGB\(255, 198, 24\), RGB\(255, 98, 26\)\}/.test(te),
      "editor: ... the same four as the firmware's TE_COL");
    ok(/SLOOP-FONT\*\/url\(data:font\/ttf;base64,[A-Za-z0-9+\/=]{20000,}\)/.test(html), "editor: the device's Terminus font inlined (tools/gen_webfont.py)");
  }
}

/* ------------------------------------------------- editor icons (Fukiai) --- */
function editorIcons() {
  const blk = html.slice(html.indexOf("const GLYPH = {"), html.indexOf("};", html.indexOf("const GLYPH = {")));
  const names = new Set([...blk.matchAll(/(\w+): 0x[0-9A-F]{4}/g)].map((m) => m[1]));
  const used = new Set([...html.matchAll(/data-ic="(\w+)"|ic: "(\w+)"|: "((?:waveform|function|symbol|control|port|ui|note)_\w+)"/g)].map((m) => m[1] || m[2] || m[3]));
  const missing = [...used].filter((n) => !names.has(n));
  ok(names.size > 0 && !missing.length, `editor: every icon name is in GLYPH (${used.size} used${missing.length ? ", missing " + missing : ""})`);
  const ttf = existsSync(join(HERE, "fukiai.ttf")) && readFileSync(join(HERE, "fukiai.ttf"));
  ok(ttf && ttf.readUInt32BE(0) === 0x00010000 && existsSync(join(HERE, "FUKIAI-LICENSE.txt")) && html.includes('href="FUKIAI-LICENSE.txt"'),
    "editor: fukiai.ttf and FUKIAI-LICENSE.txt next to editor.html");
  ok(/html:not\(\.fk\) \.ic \{ display: none; \}/.test(html) && html.includes('classList.add("fk")'), "editor: icons hidden until the font has loaded");
}

/* ------------------------------------------- user samples: JS == sampleio.py --- */
function wav(sr, ch, bits, float, frames, f) {
  const bps = bits / 8, data = Buffer.alloc(frames * ch * bps);
  for (let i = 0; i < frames; i++) for (let c = 0; c < ch; c++) {
    const v = f(i, c), o = (i * ch + c) * bps;
    if (float) data.writeFloatLE(v, o);
    else if (bits === 8) data[o] = Math.max(0, Math.min(255, Math.round(v * 127 + 128)));
    else if (bits === 16) data.writeInt16LE(Math.round(v * 32000), o);
    else if (bits === 24) data.writeIntLE(Math.round(v * 8000000), o, 3);
  }
  const fmt = Buffer.alloc(16);
  fmt.writeUInt16LE(float ? 3 : 1, 0); fmt.writeUInt16LE(ch, 2); fmt.writeUInt32LE(sr, 4);
  fmt.writeUInt32LE(sr * ch * bps, 8); fmt.writeUInt16LE(ch * bps, 12); fmt.writeUInt16LE(bits, 14);
  const chunk = (id, b) => Buffer.concat([Buffer.from(id), Buffer.from(Uint32Array.of(b.length).buffer), b]);
  const body = Buffer.concat([Buffer.from("WAVE"), chunk("fmt ", fmt), chunk("data", data)]);
  return Buffer.concat([Buffer.from("RIFF"), Buffer.from(Uint32Array.of(body.length).buffer), body]);
}

function samplesMatch() {
  const dir = mkdtempSync(join(tmpdir(), "felucca-web-"));
  const files = [
    ["tone_A4.wav", wav(44100, 1, 16, false, 9000, (i) => Math.sin(i * 0.0627) * Math.exp(-i / 4000))],
    ["pad C3.wav", wav(48000, 2, 24, false, 7000, (i, c) => Math.sin(i * (c ? 0.031 : 0.0313)) * 0.7)],
    ["Bb2 float.wav", wav(22050, 1, 32, true, 5000, (i) => ((i % 97) / 48 - 1) * 0.5)],
    ["BD1 lofi.wav", wav(96000, 1, 8, false, 12000, (i) => Math.sin(i * 0.01) * Math.exp(-i / 3000))],
  ].map(([n, b]) => { const p = join(dir, n); writeFileSync(p, b); return p; });
  const zones = files.map((p) => {
    const w = E.parseWav(readFileSync(p));
    const s = E.normalize(E.resample(w.x, w.sr, E.SMP.RATE));
    return { s, root: E.rootFromName(p.split("/").pop().replace(/\.[^.]*$/, "")) };
  });
  const js = E.buildSlot("Mix ä 12345", zones);
  execFileSync(PYTHON, [join(HERE, "../tools/fm1_sample_upload.py"), "build", "Mix ä 12345", join(dir, "slot"), ...files]);
  const pyHdr = readFileSync(join(dir, "slot.hdr")), pyData = readFileSync(join(dir, "slot.bin"));
  ok(eq(js.hdr, pyHdr) && eq(js.data, pyData), `samples: editor == sampleio.py (${files.length} WAV formats, ${js.data.length} B)`);
}

/* ------------------------------------------------------- CHOP (pure helpers) --- */
function chopTests() {
  const R = E.SMP.RATE, N = R * 4;
  /* a break: 8 hits (kick-like and snare-like, loud and ghost) on a quiet noise floor */
  let seed = 7;
  const rnd = () => ((seed = (seed * 1103515245 + 12345) >>> 0) / 2 ** 32) * 2 - 1;
  const x = new Float64Array(N);
  for (let i = 0; i < N; i++) x[i] = rnd() * 0.002;
  const HITS = [0.10, 0.52, 0.93, 1.31, 1.80, 2.26, 2.70, 3.33].map((t) => Math.round(t * R));
  const AMP = [1, 0.8, 0.25, 0.9, 1, 0.3, 0.85, 0.7];
  HITS.forEach((h, k) => {
    for (let i = 0; i < R * 0.3 && h + i < N; i++) {
      const env = Math.exp(-i / (k % 2 ? 1500 : 3000)) * AMP[k];
      x[h + i] += env * (k % 2 ? rnd() * 0.8 : Math.sin(2 * Math.PI * 60 * i / R) * 0.9 + rnd() * 0.1);
    }
  });
  const nov = E.chopNovelty(x), hits = E.chopHits(x, nov, 5);
  const near = (a, b, ms) => Math.abs(a - b) <= ms * R / 1000;
  ok(hits.length === HITS.length && hits.every((h, i) => near(h, HITS[i] - E.CHOP.PRE, 3)),
    `chop: hits found (${hits.length} of ${HITS.length}, each within 3 ms of its attack)`);
  ok(E.chopHits(x, nov, 1).length < HITS.length && E.chopHits(x, nov, 1).length >= 4, "chop: low sensitivity keeps the hard hits only");
  const late = E.chopSnap(x, nov, HITS[3] + 0.035 * R), early = E.chopSnap(x, nov, HITS[4] - 0.03 * R), none = E.chopSnap(x, nov, 1.1 * R);
  ok(near(late, HITS[3] - E.CHOP.PRE, 3) && near(early, HITS[4] - E.CHOP.PRE, 3) && none === Math.round(1.1 * R),
    "chop: a TAP 35 ms late / 30 ms early lands on its hit; away from hits it stays");
  const grid = E.chopGrid(0, Math.round(R * 60 / 90 * 16), 90, 1);
  ok(grid.length === 16 && grid[1] === Math.round(R * 60 / 90) && E.chopEqual(100, 900, 4).join() === "100,300,500,700",
    "chop: grid (16 beats at 90 BPM) and equal parts");
  const list = E.chopList([10, 50, 400], 1000, 100);
  ok(list.map((c) => `${c.start}-${c.end}`).join() === "10-50,50-150,400-500", "chop: chops end at the next marker or the max length");
  const chops = E.chopList(hits, N), zones = E.chopZones(x, chops, 60, 0);
  const peak = (s) => s.reduce((a, v) => Math.max(a, Math.abs(v)), 0);
  ok(zones.length === 8 && zones.every((z, i) => z.root === 60 + i && z.lo === z.root && z.hi === z.root && z.s[0] === 0)
    && Math.abs(peak(zones[2].s) / peak(zones[0].s) - 0.25) < 0.05 && zones[0].fname === "CHOP01_C4.wav" && zones[1].fname === "CHOP02_C#4.wav",
    "chop: one key each from C4, levels kept (a ghost stays quiet), faded in");
  const one = E.chopZones(x, chops, 60, 1, 3);
  ok(one.length === 1 && one[0].root === 60 && one[0].lo === 0 && one[0].hi === 127 && one[0].fname === "CHOP04_C4.wav",
    "chop: one chop over the whole keyboard");
  const slot = E.buildSlot("BREAK", zones), v = new DataView(slot.hdr.buffer);
  ok(slot.hdr[6] === 8 && slot.hdr[32 + 25] === 60 && slot.hdr[32 + 26] === 60 && slot.hdr[32 + 7 * 28 + 25] === 67 && v.getInt16(32 + 20, true) === 60 * 16
    && slot.data.length <= E.SMP.MAX_DATA, "chop: slot header (8 zones, one key each)");
  const w = E.parseWav(E.wavFile(zones[1].s));
  ok(w.sr === R && w.x.length === zones[1].s.length && Math.abs(w.x[100] * 32768 - zones[1].s[100]) < 2, "chop: WAV writer round trip");
  const dir = mkdtempSync(join(tmpdir(), "sloop-chop-")), zp = join(dir, "c.zip");
  writeFileSync(zp, E.zipStore(zones.slice(0, 3).map((z) => ({ name: "BREAK/" + z.fname, data: E.wavFile(z.s) }))));
  const r = py(`import sys, zipfile
z = zipfile.ZipFile(sys.argv[1]); assert z.testzip() is None
print(",".join(i.filename + ":" + str(i.file_size) for i in z.infolist()))`, zp).toString().trim();
  ok(r === zones.slice(0, 3).map((z) => `BREAK/${z.fname}:${44 + z.s.length * 2}`).join(), "chop: ZIP of the WAVs (Python reads it)");

  /* keep / leave out, own lengths, fit: a recording longer than a slot */
  const opt = [null, { off: true }, { len: 30 }];
  const kl = E.chopList([10, 50, 400], 1000, 100, opt);
  ok(kl.map((c) => `${c.start}-${c.end}${c.off ? "x" : ""}/${c.full}`).join() === "10-50/50,50-150x/400,400-430/1000"
    && E.chopList([10, 400], 1000, 0, [{ len: 9999 }])[0].end === 400,
    "chop: options (left out, own length over the max, never past the next marker)");
  const kz = E.chopZones(x, E.chopList(hits, N, 0, [{}, { off: true }, {}, { off: true }]), 60, 0);
  ok(kz.length === 6 && kz.map((z) => z.root).join() === "60,61,62,63,64,65" && kz[1].fname === "CHOP03_C#4.wav" && kz[2].fname === "CHOP05_D4.wav",
    "chop: left-out chops: the kept ones on consecutive keys, files keep their numbers");
  const L = R * 20, long = new Float64Array(L);
  for (let i = 0; i < L; i++) long[i] = Math.sin(i * 0.05) * 0.5;
  const marks = E.chopEqual(0, L, 40), room = E.SMP.MAX_DATA * 2;
  const all = E.chopList(marks, L), pick = E.chopPick(all);
  ok(pick.length === 16 && pick[15].i === 15 && E.chopPick(all, 1, 7)[0].i === 7, "chop: 40 chops: the first 16 kept go to the slot; mode 1 the selected");
  const lo = E.chopList(marks, L, 0, marks.map((_, i) => ({ off: i % 4 !== 0 })));   /* keep 10 of 40 (each 0.5 s) */
  ok(E.chopPick(lo).length === 10 && E.chopFit(E.chopPick(lo), room) === Infinity, "chop: 20 s recording, 10 chops kept: they fit");
  const many = E.chopPick(E.chopList(marks, L, 0, marks.map((_, i) => ({ off: i >= 16 })))), Lf = E.chopFit(many, room - many.length);
  const fitted = many.map((c) => ({ ...c, end: Math.min(c.end, c.start + Lf) }));
  let built = null;
  try { built = E.buildSlot("LONG", E.chopZones(long, fitted, 60, 0)); } catch (e) { built = null; }
  ok(Lf < R * 0.5 && Lf > R * 0.4 && built && built.data.length <= E.SMP.MAX_DATA && built.hdr[6] === 16,
    `chop: Fit to slot: 16 x 0.5 s cut to ${(Lf / R).toFixed(3)} s each, the slot builds`);
  ok(E.chopFit([{ start: 0, end: 100 }, { start: 0, end: 300 }], 250) === 150, "chop: fit keeps short chops whole, cuts the long ones");
}

/* ------------------------------------------------------- packages: JS == Python --- */
async function packages() {
  const pkg = join(HERE, "../build/felucca.fwsc");
  if (!existsSync(pkg)) {
    console.log("packages: skipped (run ./build.sh first)");
    return;
  }
  const raw = readFileSync(pkg);
  const logical = py(`import sys; raw = open(sys.argv[1], "rb").read()
sys.stdout.buffer.write(b"".join(raw[i * 48:i * 48 + 47] for i in range(20)) + raw[960:])`, pkg);
  ok(eq(logicalImage(raw), logical), "fm1pkg.js logicalImage");
  ok(/^FM-1_9\d\d$/.test(productOf(raw)), "fm1pkg.js productOf");
}

/* ------------------------------------------------- update protocol (fm1ota.js) --- */
const HS = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
const UPGRADE = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7];

/* an FM-1 on WebMIDI: identity, then "device asks, host answers" reads of the image */
class FakeFM1 {
  constructor(image, { unplugAfter = Infinity, finalIdentity = "FM-1_900", name = "FM-1", identity = "FM-1_015" } = {}) {
    this.image = image; this.unplugAfter = unplugAfter; this.served = 0; this.bad = 0;
    this.finalIdentity = finalIdentity;
    this.access = { inputs: new Map(), outputs: new Map() };
    this.boot(identity, name);
  }
  boot(identity, name) {
    this.identity = identity; this.waiting = null; this.queue = [];
    for (const m of [this.access.inputs, this.access.outputs]) { for (const p of m.values()) p.state = "disconnected"; m.clear(); }
    const id = Math.random().toString(36).slice(2);
    this.input = { id: "i" + id, name, state: "connected", onmidimessage: null, open: async () => {} };
    this.output = { id: "o" + id, name, state: "connected", open: async () => {}, send: (d) => {
      if (this.output.state !== "connected") throw new Error("InvalidStateError");
      setTimeout(() => this.rx(Array.from(d)), 1);
    } };
    this.access.inputs.set(this.input.id, this.input);
    this.access.outputs.set(this.output.id, this.output);
  }
  tx(bytes) { const i = this.input; setTimeout(() => { if (i.state === "connected" && i.onmidimessage) i.onmidimessage({ data: Uint8Array.from(bytes) }); }, 1); }
  rx(d) {
    if (eq(d, HS)) {
      const t = [...new TextEncoder().encode(this.identity)];
      const body = [0, 0x59, 0x11, 0, 0, 0, ...t, ...new Array(28 - t.length).fill(0)];
      this.tx([0xF0, ...pack7(body), 0xF7]);
    } else if (eq(d, UPGRADE)) {
      this.queue = this.identity.startsWith("ota-")
        ? [...Array.from({ length: 6 }, (_, k) => [k * 512, 512]), [0xF0000000, 8]]
        : [[0, 64], [0x40, 160], [0x1000, 512], [0xE0000000, 8]];
      this.next();
    } else if (this.waiting) {
      const u = unpack7(d.slice(1, -1));
      const [addr, len] = this.waiting;
      const got = u.slice(14, 14 + (addr >= 0xE0000000 ? 8 : len));
      const want = addr >= 0xE0000000 ? [...new TextEncoder().encode("success"), 0] : Array.from(this.image.subarray(addr, addr + len));
      if (!eq(got, want)) this.bad++;
      this.waiting = null;
      this.served++;
      if (this.served >= this.unplugAfter) { this.input.state = this.output.state = "disconnected"; return; }
      if (addr === 0xE0000000) setTimeout(() => this.boot("ota-FM-1_900", "Felucca Update"), 300);
      else if (addr === 0xF0000000) setTimeout(() => this.boot(this.finalIdentity, "Felucca"), 300);
      else this.next();
    }
  }
  next() {
    const r = this.queue.shift();
    if (!r) return;
    this.waiting = r;
    const [addr, len] = r;
    const u = [0, 0x59, 0x30, 0, 0, 0, 0, addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF, len & 0xFF, len >> 8, 0];
    let s = 0;
    for (let i = 6; i < 14; i++) s += u[i];
    u.push(~s & 0xFF);
    this.tx([0xF0, ...pack7(u), 0xF7]);
  }
}

async function updater() {
  const image = Uint8Array.from({ length: 0x2000 }, (_, i) => (i * 7) & 0xFF);
  const dev = new FakeFM1(image);
  const steps = [];
  const got = await new Updater(dev.access).install(image, "FM-1_900", (k) => steps.push(k));
  ok(got === "FM-1_900" && dev.bad === 0 && steps.includes("write") && steps.at(-1) === "done",
    `fm1ota.js: install: running firmware -> loader -> Felucca (${dev.served} reads)`);

  const rescue = new FakeFM1(image);
  rescue.boot("FM-1_000", "Felucca");
  const recovered = await new Updater(rescue.access).install(image, "FM-1_900");
  ok(recovered === "FM-1_900" && rescue.bad === 0, "fm1ota.js: recovery mode -> loader -> normal firmware");
  const failedBoot = new FakeFM1(image, { finalIdentity: "FM-1_000" });
  const rescueSteps = [];
  const bootError = await new Updater(failedBoot.access).install(image, "FM-1_900", (k) => rescueSteps.push(k)).then(() => null, (e) => e);
  ok(bootError?.code === "mismatch" && !rescueSteps.includes("done"), "fm1ota.js: boot into recovery is not reported as successful installation");

  const dev2 = new FakeFM1(image, { unplugAfter: 3 });
  dev2.boot("ota-FM-1_900", "Felucca Update");
  const t0 = Date.now();
  const done = await new Updater(dev2.access).resume(image);
  ok(done === false && Date.now() - t0 < 6000, "fm1ota.js: unplugged during the write -> stops at once");

  const dev3 = new FakeFM1(image, { unplugAfter: 2 });
  const e = await new Updater(dev3.access).install(image, "FM-1_900").then(() => null, (x) => x);
  ok(e && e.code === "lost", "fm1ota.js: unplugged in step 1 -> error code 'lost'");
  const e2 = await new Updater({ inputs: new Map(), outputs: new Map() }).install(image, "FM-1_900").then(() => null, (x) => x);
  ok(e2 && e2.code === "notfound", "fm1ota.js: no device -> error code 'notfound'");
  {   /* an FM-1 running CLIPPER (USB name "Clipper", SLOOP's sister firmware) installs SLOOP like any FM-1 */
    const cl = new FakeFM1(image, { name: "Clipper", identity: "FM-1_901" });
    const g = await new Updater(cl.access).install(image, "FM-1_900");
    ok(g === "FM-1_900" && cl.bad === 0, "fm1ota.js: an FM-1 under CLIPPER (port 'Clipper') -> found and installed");
    /* other gear is never probed, and "not found" names the MIDI ports there are */
    const other = new FakeFM1(image, { name: "Arturia KeyStep Pro" });
    const e6 = await new Updater(other.access).install(image, "FM-1_900").then(() => null, (x) => x);
    ok(e6 && e6.code === "notfound" && other.served === 0 && /Arturia KeyStep Pro/.test(e6.detail),
       `fm1ota.js: a port of other gear is left alone; 'not found' lists it${e6 ? e6.detail : ""}`);
    ok(["FM-1", "Felucca", "Felucca Update", "Clipper", "SLOOP", "MIDIIN2 (Clipper)", "USB-MIDI"].every((n) => FM1_PORT.test(n)) &&
       !["Arturia KeyStep Pro", "loopMIDI Port", "Microsoft GS Wavetable Synth"].some((n) => FM1_PORT.test(n)),
       "fm1ota.js: FM1_PORT takes the FM-1's names (SLOOP, CLIPPER, the loader, the stock firmware), not other gear");
  }
  const stock = new FakeFM1(image);
  stock.boot("ota-FM-1_015", "FM-1 Update");
  const e3 = await new Updater(stock.access).resume(image).then(() => null, (x) => x);
  ok(e3 && e3.code === "foreign" && e3.detail === "ota-FM-1_015" && stock.served === 0, "fm1ota.js: another firmware's loader is never resumed ('foreign')");
  const back = new FakeFM1(image, { finalIdentity: "FM-1_015" });   /* the return to the official V15, interrupted */
  back.boot("ota-FM-1_015", "FM-1 Update");
  const st = [];
  const r4 = await new Updater(back.access).resume(image, (k) => st.push(k), { product: "FM-1_015" });
  ok(r4 === true && back.bad === 0 && st.at(-1) === "done", "fm1ota.js: return to official: its own loader resumed, V15 checked when back");
  const wrong = new FakeFM1(image, { finalIdentity: "FM-1_900" });
  wrong.boot("ota-FM-1_015", "FM-1 Update");
  const e5 = await new Updater(wrong.access).resume(image, null, { product: "FM-1_015" }).then(() => null, (x) => x);
  ok(e5 && e5.code === "mismatch", "fm1ota.js: return to official: another firmware coming back is not 'done'");
  const pk = await import(join(HERE, "fm1pkg.js"));
  const notStock = new Uint8Array(pk.STOCK_V15_SIZE);
  const e6 = await pk.validateStockPackage(notStock).then(() => null, (x) => x);
  ok(e6 && /official FM-1 V15/.test(e6.message), "fm1pkg.js: only the exact official V15 is accepted (SHA-256)");
}

await editorMock();
await editorLibrarian();
await editorLive();
await editorTracks();
await editorSetGate();
await editorMixer();
await editorTrackParam();
await editorV5();
await editorV7();
await editorV8();
await editorV9();
await editorBackup();
await editor25();
await editorDsyn();
await editorCart();
editorTabs();
editorIcons();
samplesMatch();
chopTests();
await packages();
await updater();
console.log(failed ? `WEB TESTS FAILED (${failed})` : "web tests passed");
process.exit(failed ? 1 : 0);
