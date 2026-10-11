// SPDX-License-Identifier: GPL-3.0-only
// SLOOP live: the shell. Plug the FM-1 in and it connects (after the first time, no button); unplug it and the
// page stays as it was, waiting for it to come back. Four tabs: Live, Arrange, Sound, Mix.
import { PF } from "./proto.js";
import { Device, AutoMidi } from "./device.js";
import { WebMidiTransport, WebSocketTransport } from "./transports.js";
import { $, h, sheet, toast } from "./widgets.js";
import { SEC, position } from "./parts.js";
import { liveTab } from "./live.js";
import { arrangeTab } from "./arrange.js";
import { soundTab } from "./sound.js";
import { mixTab } from "./mix.js";
import { helpSheet, GUIDE_URL } from "./help.js";

const APP_VERSION = "1.1";                            /* SLOOP live's own version (the setup sheet shows it) */
const QS = new URLSearchParams(location.search);
const dev = new Device();
let tabs = null, auto = null, wakeLock = null, mode = "usb";
const remember = { get: () => { try { return localStorage.getItem("sloop.usb") === "1"; } catch (_) { return false; } },
  set: () => { try { localStorage.setItem("sloop.usb", "1"); } catch (_) {} } };
const TABS = ["live", "arrange", "sound", "mix"];
let current = (() => { try { return localStorage.getItem("sloop.tab") || "live"; } catch (_) { return "live"; } })();

/* ------------------------------------------------------------ screens --- */
function showGate(msg = "", err = false) {
  $("gate").hidden = false;
  $("app").hidden = true;
  const m = $("gatemsg");
  m.textContent = msg;
  m.classList.toggle("err", err);
}
function showApp() { $("gate").hidden = true; $("app").hidden = false; }
/* no FM-1 yet (or unplugged before anything was read): the plug-in card instead of the tabs */
function showStandby(text) {
  showApp();
  $("standby").hidden = false;
  $("tabs").hidden = true;
  $("app").classList.add("standing");
  $("standbymsg").textContent = text;
  paintStatus();
}

function build() {
  const host = $("tabs");
  tabs = { live: liveTab(dev), arrange: arrangeTab(dev), sound: soundTab(dev), mix: mixTab(dev) };
  host.replaceChildren(...TABS.map((k) => tabs[k].el));
  tabs.mix.build();
  tabs.sound.rebuild();
  show(current);
  $("standby").hidden = true;
  $("app").classList.remove("standing");
  host.hidden = false;
  paintAll();
  $("app").classList.add("boot");                     /* (the hello: a wave of light across the pads) */
  setTimeout(() => $("app").classList.remove("boot"), 1200);
}
/* a tablet (or any big screen) splits: Live stays on screen, the tab bar picks the other pane (Sound first) */
const SPLIT = matchMedia("(min-width: 1000px) and (min-height: 600px), (min-width: 700px) and (min-height: 900px)");
let split = SPLIT.matches;
let side = (() => { try { const v = localStorage.getItem("sloop.side"); return TABS.includes(v) && v !== "live" ? v : "sound"; } catch (_) { return "sound"; } })();
SPLIT.addEventListener("change", (e) => { split = e.matches; show(split ? side : current); });
const shown = () => (split ? ["live", side] : [current]);

function show(k) {
  if (split) { if (k !== "live") side = k; } else current = k;
  try { localStorage.setItem("sloop.tab", current); localStorage.setItem("sloop.side", side); } catch (_) {}
  const vis = shown();
  $("app").classList.toggle("split", split);
  for (const t of TABS) {
    if (tabs) tabs[t].el.hidden = !vis.includes(t);
    $("nav-" + t).setAttribute("aria-selected", split ? t === side : t === current);
  }
  if (tabs) paintAll();
}
function paintAll() {
  if (!tabs) return;
  paintTop();
  for (const t of shown()) tabs[t].paint();
}

/* ------------------------------------------------------------ the top bar --- */
function paintTop() {
  const s = dev.live, play = $("play");
  play.classList.toggle("on", !!(s && s.playing));
  play.setAttribute("aria-label", s && s.playing ? "Stop" : "Play");
  play.disabled = !dev.perform;
  $("bpm").textContent = s ? s.bpm : dev.g && dev.g.BPM ? dev.g.BPM.value : "";
  $("bpm").disabled = !dev.perform;
  const pos = position(s);
  document.querySelectorAll(".beats i").forEach((d, i) => d.classList.toggle("on", !!pos && pos.beat === i + 1));
  $("where").textContent = !s ? "" : !s.playing ? "stopped" : (s.section >= 0 ? SEC[s.section] + " " : "") +
    (pos.bars ? `bar ${pos.bar}/${pos.bars}` : `bar ${pos.bar}`) + (s.next >= 0 ? `  > ${SEC[s.next]}` : "");
  paintStatus();
}
function paintStatus() {
  const st = $("status");
  const lost = dev.status === "lost" || (dev.connected && dev.stale);
  st.className = "status " + (dev.connected && !dev.stale ? "ok" : lost ? "bad" : "wait");
  st.textContent = dev.connected ? (dev.stale ? "no reply" : mode === "demo" ? "demo" : mode === "ws" ? "relay" : "usb")
    : dev.status === "lost" ? "unplugged" : "waiting";
  $("app").classList.toggle("lost", dev.status === "lost");
}

/* ------------------------------------------------------------ device events --- */
dev.addEventListener("connection", (e) => {
  const s = e.detail.status;
  if (s === "connected") {
    $("banner").hidden = dev.perform;
    $("banner").textContent = dev.perform ? "" : `${dev.info.version} speaks protocol v${dev.info.proto}. The mix and the sounds work; the pads, sections and transport need SLOOP with protocol v11.`;
    build();
    keepAwake();
    if (mode === "usb") remember.set();
    try {                                              /* once: where the help is */
      if (!localStorage.getItem("sloop.helped")) { localStorage.setItem("sloop.helped", "1"); toast("New here? Tap ? in the top bar to see how each tab is played.", 4500); }
    } catch (_) {}
  } else if (s === "lost") {
    tabs && tabs.live.release();
    if (tabs) { $("banner").hidden = false; $("banner").textContent = "The FM-1 is unplugged. Plug it back in to carry on: this page reconnects by itself."; }
    else showStandby("Plug the FM-1 back in.");
  }
  paintStatus();
});
dev.addEventListener("state", () => paintAll());
dev.addEventListener("health", () => paintStatus());
dev.addEventListener("tracks", () => { if (tabs) for (const t of shown()) tabs[t].paint(); });
dev.addEventListener("sound", (e) => {
  if (!tabs) return;
  if (e.detail && e.detail.id !== undefined) { tabs.sound.paintValues(e.detail); tabs.live.paintValues(); return; }
  tabs.sound.rebuild();
  tabs.live.paintValues();
});
dev.addEventListener("globals", () => { if (!tabs) return; tabs.mix.paint(); tabs.live.paintValues(); tabs.sound.paintValues(); });

/* ------------------------------------------------------------ connecting --- */
async function connectUsb() {
  if (!auto.present()) { if (!dev.connected) showStandby("Plug the FM-1 in with a USB data cable (an OTG adapter on a phone)."); return; }
  if (dev.connected) return;
  try {
    mode = "usb";
    await dev.connect(new WebMidiTransport(auto.access));
    $("banner").hidden = dev.perform;
  } catch (e) {
    console.error(e);
    if (!tabs) showStandby(/timeout/.test(e.message) ? "The FM-1 did not answer. Is SLOOP installed? Unplug it and plug it back in." : e.message);
  }
}
async function startUsb() {
  try {
    auto = auto || new AutoMidi(() => connectUsb());
    await auto.start();
  } catch (e) {
    showGate("MIDI access was refused. In Chrome: the lock icon in the address bar > Site settings > MIDI devices > Allow, then reload.", true);
    return;
  }
  remember.set();
  connectUsb();
}
async function startDemo() {
  const { MockTransport } = await import("./mock.js");
  mode = "demo";
  showStandby("Starting the demo...");
  await dev.connect(new MockTransport(+QS.get("mock") === 10 ? 10 : 11));
}
async function startRelay(url) {
  mode = "ws";
  showStandby("Connecting to the relay...");
  try { await dev.connect(new WebSocketTransport(url)); } catch (e) { showStandby(e.message); }
}

async function keepAwake() {
  try {
    if ("wakeLock" in navigator && !wakeLock) {
      wakeLock = await navigator.wakeLock.request("screen");
      wakeLock.onrelease = () => { wakeLock = null; };
    }
  } catch (_) {}
}
document.addEventListener("visibilitychange", () => {
  if (document.hidden) { if (tabs) tabs.live.release(); }
  else if (dev.connected) keepAwake();
});

/* ------------------------------------------------------------ the setup sheet --- */
function setupSheet() {
  const i = dev.info;
  const close = sheet("SLOOP live", [
    h("p", {}, dev.connected ? `${i.version}, protocol v${i.proto}, over ${mode === "demo" ? "the demo device" : mode === "ws" ? "the relay" : "USB"}.`
      : "Not connected."),
    h("p", { class: "dim" }, `SLOOP live ${APP_VERSION}. Online it always opens the latest version; offline, the one it kept.`),
    h("p", { class: "dim" }, "Install it: Chrome menu > Add to Home screen. It opens full screen and keeps the phone awake while connected."),
    h("div", { class: "row two" },
      mode === "demo" || !AutoMidi.supported ? h("button", { class: "chip big", onclick: () => { location.search = ""; } }, "leave the demo")
        : h("button", { class: "chip big", onclick: () => { close(); dev.disconnect("idle"); startDemo(); } }, "try the demo"),
      h("button", { class: "chip big", onclick: () => { close(); updateNow(); } }, "update and reload")),
    h("a", { class: "chip big guide", href: GUIDE_URL, target: "_blank", rel: "noopener" }, "Open the user guide"),
    h("p", { class: "dim" }, "SLOOP live is GPL-3.0, part of SLOOP (based on Felucca by Leo Kuroshita, Hügelton Instruments)."),
  ]);
}

/* ------------------------------------------------------------ updates --- */
/* The service worker answers from the network first, so a reload is always the latest. An installed app has no
   reload, though: when a new version of the worker takes over, a bar offers one (never on its own: a reload in the
   middle of a jam would drop the FM-1 for a second). The worker is checked whenever the app comes back to the front. */
let swReg = null;
function registerWorker() {
  if (!("serviceWorker" in navigator) || location.protocol === "file:") return;
  let had = !!navigator.serviceWorker.controller;        /* (the first install takes over too: no bar for that one) */
  navigator.serviceWorker.addEventListener("controllerchange", () => { if (had) $("update").hidden = false; had = true; });
  navigator.serviceWorker.register("sw.js").then((reg) => {
    swReg = reg;
    document.addEventListener("visibilitychange", () => { if (!document.hidden) reg.update().catch(() => {}); });
    setInterval(() => reg.update().catch(() => {}), 30 * 60 * 1000);
  }).catch(() => {});
}
async function updateNow() {
  if (tabs) tabs.live.release();
  try { if (swReg) await swReg.update(); } catch (_) {}
  location.reload();
}

/* ------------------------------------------------------------ wiring --- */
$("updatebtn").addEventListener("click", updateNow);
$("updatelater").addEventListener("click", () => { $("update").hidden = true; });
for (const t of TABS) $("nav-" + t).addEventListener("click", () => show(t));
$("play").addEventListener("click", async () => {
  const s = await dev.op(PF.TRANSPORT, [0]);
  if (s && s.rc === 1) toast("Song mode, but a section of the song is empty.");
});
$("bpm").addEventListener("pointerdown", () => dev.op(PF.TAP));
$("status").addEventListener("click", setupSheet);
$("help").addEventListener("click", () => helpSheet(tabs ? shown() : ["live"]));
$("connect").addEventListener("click", startUsb);
$("demo").addEventListener("click", startDemo);
$("standbydemo").addEventListener("click", () => { dev.disconnect("idle"); startDemo(); });

(async () => {
  if (QS.get("mock")) { startDemo(); }
  else if (QS.get("ws")) { startRelay(QS.get("ws")); }
  else if (!AutoMidi.supported) {
    $("connect").disabled = true;
    showGate("This browser has no Web MIDI (Safari on iPhone and iPad does not). Use Chrome on Android, or Chrome or Edge on a computer. The demo works anywhere.");
  } else {
    const perm = await AutoMidi.permission();
    if (perm === "granted" || (remember.get() && perm !== "denied")) { showStandby("Plug the FM-1 in with a USB data cable (an OTG adapter on a phone)."); startUsb(); }
    else if (perm === "denied") showGate("MIDI access is blocked for this site. In Chrome: the lock icon in the address bar > Site settings > MIDI devices > Allow, then reload.", true);
    else showGate();
  }
  registerWorker();
})();
