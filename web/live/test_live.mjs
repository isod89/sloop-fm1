// SPDX-License-Identifier: GPL-3.0-only
// SLOOP live without a browser: the frames, the link (one request in flight, coalescing, the pads ahead of
// the polls) and the PERFORM reply, against the demo device (mock.js).   node web/live/test_live.mjs
import { CMD, PF, NONE, Link, frame, unframe, v14, parseInfo, parsePerform, parseTrack, parseDump, parseNames, parseUpList, fmtValue, F } from "./proto.js";
import { MockTransport } from "./mock.js";
import { Device, TP_IDS } from "./device.js";

let fails = 0;
const check = (ok, what) => { console.log(`live: ${what.padEnd(70)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const wait = (ms) => new Promise((r) => setTimeout(r, ms));

check(frame(CMD.PERFORM, [PF.FX, 7]).join(",") === "240,125,70,76,43,1,7,247", "PERFORM FX 7 framing");
check(v14(120).join(",") === [(120 + 8192) & 127, (120 + 8192) >> 7].join(","), "v14");
check(unframe(Uint8Array.from([0xf0, 0x43, 1, 2, 3, 0xf7])) === null, "another maker's SysEx: not ours");

const mock = new MockTransport();
let link;
await mock.open((d) => link.receive(d), () => {});
const sent = [];
link = new Link((d) => { sent.push(unframe(d)); mock.send(d); });
const info = parseInfo(await link.request(CMD.INFO));
check(info.proto === 11 && info.nTrk === 4 && info.engines.length >= 12 && info.engines[0] === "ANALOG", "INFO: protocol 11, 4 tracks, engine names");
const tr = parseTrack(await link.request(CMD.TRACK));
check(tr.tracks.length === 4 && tr.tracks[2].mute, "TRACK: four tracks, 3 muted");

/* one in flight: three requests, queued in order; a polled STATE coalesces; a pad goes first */
sent.length = 0;
const s1 = link.request(CMD.PERFORM, [PF.STATE], { key: "state" });
link.request(CMD.PERFORM, [PF.STATE], { key: "state" });
const s2 = link.request(CMD.PERFORM, [PF.STATE], { key: "state" });
const fx = link.request(CMD.PERFORM, [PF.FX, 4], { front: true, key: "fx" });
link.request(CMD.PERFORM, [PF.FX, 5], { front: true, key: "fx" });  /* the latest pad wins */
check(link.q.length === 2 && link.q[0].args[1] === 5, "queued: the pad first (latest wins), one STATE");
await Promise.all([s1, s2, fx]);
check(sent.map((f) => f.a.join(":")).join(" ") === "0 1:5 0", "sent: STATE, FX 5, STATE");
let st = parsePerform(await link.request(CMD.PERFORM, [PF.STATE]));
check(st.fx === 5 && st.fxRemote && st.playing, "the state: FX 5 held from the remote, playing");
check(st.sel === 0 && st.bars && st.bars.join(",") === "1,2,0,0", "the state: selected track, section lengths (C, D empty)");
st = parsePerform(await link.request(CMD.PERFORM, [PF.FX, NONE]));
check(st.op === PF.FX && st.rc === 0 && st.fx === -1, "FX off");
st = parsePerform(await link.request(CMD.PERFORM, [PF.CHAIN, 3, 0, 1, 1]));
check(st.rc === 0 && st.chain.join("") === "011" && st.next === 0, "CHAIN A B B");
st = parsePerform(await link.request(CMD.PERFORM, [PF.CHAIN, 1, 3]));
check(st.rc === 2, "a chain with an empty D: rc 2");
st = parsePerform(await link.request(CMD.PERFORM, [PF.SOLO, 1, 1]));
check(st.solo === 2, "solo 2");
/* a quiet remote lets go of its FX (1.5 s) */
await link.request(CMD.PERFORM, [PF.FX, 9]);
await wait(1900);
st = parsePerform(await link.request(CMD.PERFORM, [PF.STATE]));
check(st.fx === -1, "quiet for 1.9 s: the FX let go");
/* a reply that is not the current request's is not taken for it */
const g = link.request(CMD.GET, [1, 0]);
link.receive(frame(CMD.GET, [1, 5, ...v14(3)]));
check(link.cur && link.cur.cmd === CMD.GET, "a GET reply for another id: still waiting");
await g;
const dump = parseDump(await link.request(CMD.TRACK_DUMP, [3]), info.pCount);
check(dump.track === 3 && dump.eng === info.nEng && dump.p.length === info.pCount, "TRACK_DUMP: the drum track, every parameter");
const nm = parseNames(await link.request(CMD.NAMES, [0]));
check(nm.eng === 0 && nm.names.length > 10 && nm.pages.join(",") === "OSC,FLT", "NAMES: analog presets and its two page titles");
const ul = parseUpList(await link.request(CMD.UP_LIST, [0, 16]));
check(ul.total === 32 && ul.slots.length === 16 && ul.slots[0].used && ul.slots[0].name === "MY WOBBLE" && !ul.slots[5].used, "UP_LIST: used and free slots");
check(fmtValue({ fmt: F.FILT, min: -64, max: 63 }, -32) === "low 50%" && fmtValue({ fmt: F.SWING }, 24) === "56%" &&
  fmtValue({ fmt: F.ENUM, min: 0, names: ["SAW", "SQR"] }, 1) === "sqr" && fmtValue({ fmt: F.BIPCT, min: -64, max: 63 }, 0) === "0%", "values as the device says them");
mock.close();
link.close();

/* the Device: connect, the selected track's sound, presets, knobs, user presets, track selection */
{
  const dev = new Device();
  await dev.connect(new MockTransport());
  await wait(50);
  check(dev.connected && dev.perform && dev.g.FILT && dev.g.DUST && dev.g.BPM.value === 96, "Device: connected, the master parameters found by label");
  check(dev.sound && dev.sound.track === 0 && !dev.sound.drum && dev.sound.edit.length === 8 && dev.sound.edit[4].label === "CUT", "Device: the selected track's sound and its engine's knobs");
  check(dev.tdesc[TP_IDS.DLY] && dev.tdesc[TP_IDS.DLY].label === "DLY" && dev.tdesc[TP_IDS.TFLT].label === "FILT", "Device: track parameters checked by their labels");
  const cut = dev.info.pE0 + 4;
  dev.setParam(cut, 30);
  await wait(60);
  const d2 = parseDump(await dev.link.request(CMD.TRACK_DUMP, [0]), dev.info.pCount);
  check(d2.p[cut] === 30, "Device: a knob reaches the device");
  const p0 = dev.sound.preset;
  await dev.preset(0, p0 + 1);
  check(dev.sound.preset === p0 + 1 && dev.sound.p[cut] !== 30, "Device: the next preset, its values read back");
  const ups = await dev.userPresets();
  check(ups.length === 32 && ups.filter((u) => u.used).length === 2, "Device: 32 user slots, 2 used");
  const rc = await dev.storeUser(5, "TEST SOUND");
  const ups2 = await dev.userPresets();
  check(rc === 0 && ups2[5].used && ups2[5].name === "TEST SOUND", "Device: keep a sound in a free slot");
  await dev.selectTrack(3);
  await wait(40);
  check(dev.sel === 3 && dev.sound.track === 3 && dev.sound.drum && dev.desc(dev.info.pE0).label === "KIT", "Device: select the drum track, its kit list");
  let lost = false;
  dev.addEventListener("connection", (e) => { if (e.detail.status === "lost") lost = true; });
  dev.disconnect("lost");
  check(lost && !dev.connected, "Device: unplugged");
}
console.log(fails ? `live: ${fails} FAIL` : "live: all ok");
process.exit(fails ? 1 : 0);
