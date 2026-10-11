// SPDX-License-Identifier: GPL-3.0-only
// LIVE: what the hands want while the loop plays. Sections, mute / solo, the 16 punch-in pads, and the XY pad
// (by default the DJ filter across, dust up), which springs back when let go.
import { PF, NONE, FX_NAMES, fmtValue } from "./proto.js";
import { TP_IDS } from "./device.js";
import { h, holdable, xyPad, sheet, buzz, toast } from "./widgets.js";
import { TC, sectionTiles, trackTiles } from "./parts.js";

const store = {
  get(k, d) { try { const v = localStorage.getItem("sloop." + k); return v === null ? d : JSON.parse(v); } catch (_) { return d; } },
  set(k, v) { try { localStorage.setItem("sloop." + k, JSON.stringify(v)); } catch (_) {} },
};

/* what an XY axis can move: the master (globals) or the selected track's sound */
function targets(dev) {
  const list = [
    { key: "g:FILT", name: "filter", kind: "g", label: "FILT" },
    { key: "g:DUST", name: "dust", kind: "g", label: "DUST" },
    { key: "g:DUCK", name: "duck", kind: "g", label: "DUCK" },
    { key: "t:TFLT", name: "track filter", kind: "t", id: TP_IDS.TFLT },
    { key: "t:DLY", name: "delay send", kind: "t", id: TP_IDS.DLY },
    { key: "t:REV", name: "reverb send", kind: "t", id: TP_IDS.REV },
    { key: "t:DST", name: "drive", kind: "t", id: TP_IDS.DST },
  ];
  if (dev.sound && !dev.sound.drum) {
    dev.sound.edit.forEach((d, k) => {
      if (d.label && d.label !== "-" && d.fmt !== 8) list.push({ key: "e:" + k, name: d.label.toLowerCase(), kind: "t", id: dev.info.pE0 + k });
    });
  }
  return list;
}
/* a target's descriptor, value, and setter */
function bind(dev, t) {
  if (!t) return null;
  if (t.kind === "g") {
    const p = dev.g[t.label];
    return p ? { desc: p.desc, get: () => p.value, set: (v) => dev.setGlobal(t.label, v) } : null;
  }
  const d = dev.sound && dev.desc(t.id);
  if (!d || (dev.sound.drum && t.id >= dev.info.pE0)) return null;
  return { desc: d, get: () => dev.sound.p[t.id], set: (v) => dev.setParam(t.id, v) };
}

export function liveTab(dev) {
  const secs = sectionTiles(dev);
  const trks = trackTiles(dev);

  /* ---- punch-in pads: hold; two fingers, the last one plays; latch: a tap toggles ---- */
  const held = [];
  let latch = store.get("latch", false);
  const fxSend = () => dev.op(PF.FX, [held.length ? held[held.length - 1] : NONE], "fx");
  const pads = FX_NAMES.map((n, i) => {
    const b = h("button", { class: "pad" + (i % 4 === 0 ? " mark" : ""), "--c": TC[i >> 2], "--i": (i >> 2) + (i & 3), "aria-label": "Punch-in " + n },
      h("b", {}, String(i + 1)), n);
    holdable(b, () => {
      if (latch) {
        const on = dev.live && dev.live.fx === i;
        held.length = 0;
        if (!on) held.push(i);
      } else {
        const k = held.indexOf(i);
        if (k >= 0) held.splice(k, 1);
        held.push(i);
      }
      buzz(8);
      fxSend();
      paintPads();
    }, () => {
      if (latch) return;
      const k = held.indexOf(i);
      if (k < 0) return;
      held.splice(k, 1);
      fxSend();
      paintPads();
    });
    return b;
  });
  const latchBtn = h("button", { class: "chip", "aria-pressed": latch, onclick: () => {
    latch = !latch;
    store.set("latch", latch);
    latchBtn.setAttribute("aria-pressed", latch);
    held.length = 0;
    fxSend();
    paintPads();
  } }, "latch");
  const paintPads = () => {
    const s = dev.live, fx = s ? s.fx : -1, remote = s && s.fxRemote;
    pads.forEach((b, i) => {
      const mine = held.length ? held[held.length - 1] === i : remote && fx === i;
      b.classList.toggle("on", !!mine);
      b.classList.toggle("dev", !mine && fx === i);        /* played on the FM-1's own keys */
      b.setAttribute("aria-pressed", !!mine);
      b.disabled = !dev.perform;
    });
  };

  /* ---- the XY pad ---- */
  let ax = store.get("xy.x", "g:FILT"), ay = store.get("xy.y", "g:DUST"), spring = store.get("xy.spring", true);
  let pre = null, cur = null;
  const tgt = (k) => targets(dev).find((t) => t.key === k) || null;
  const norm = (b) => (b ? (b.get() - b.desc.min) / (b.desc.max - b.desc.min || 1) : 0.5);
  const axisLab = h("button", { class: "chip wide", "aria-label": "Choose what the XY pad moves", onclick: () => pickAxes() });
  const readout = h("div", { class: "xyread" });
  const xyEl = h("div", { class: "xypad" }, readout);
  const pad = xyPad(xyEl, {
    get: () => cur || [norm(bind(dev, tgt(ax))), norm(bind(dev, tgt(ay)))],
    onDown: () => {
      const bx = bind(dev, tgt(ax)), by = bind(dev, tgt(ay));
      pre = [bx && bx.get(), by && by.get()];
      buzz(6);
    },
    onMove: (x, y) => {
      cur = [x, y];
      const bx = bind(dev, tgt(ax)), by = bind(dev, tgt(ay));
      const put = (b, f) => {
        if (!b) return;
        let v = b.desc.min + f * (b.desc.max - b.desc.min);
        if (b.desc.min < 0 && b.desc.max > 0 && Math.abs(v) < (b.desc.max - b.desc.min) * 0.04) v = 0;   /* (a dead zone at the centre) */
        b.set(Math.round(v));
      };
      put(bx, x);
      put(by, y);
      paintRead();
    },
    onUp: () => {
      if (spring && pre) {
        const bx = bind(dev, tgt(ax)), by = bind(dev, tgt(ay));
        if (bx && pre[0] !== null) bx.set(pre[0]);
        if (by && pre[1] !== null) by.set(pre[1]);
      }
      cur = null;
      pre = null;
      paintRead();
    },
  });
  const paintRead = () => {
    const tx = tgt(ax), ty = tgt(ay), bx = bind(dev, tx), by = bind(dev, ty);
    axisLab.textContent = `${tx ? tx.name : "none"} ↔ ${ty ? ty.name : "none"} ↕` + (spring ? "" : "  (stays)");
    readout.textContent = [bx && `${tx.name} ${fmtValue(bx.desc, bx.get())}`, by && `${ty.name} ${fmtValue(by.desc, by.get())}`].filter(Boolean).join("\n");
    xyEl.classList.toggle("off", !bx && !by);
    const [x, y] = pad ? (cur || [norm(bx), norm(by)]) : [0.5, 0.5];
    readout.classList.toggle("right", x < 0.5);          /* (the readout keeps clear of the finger) */
    readout.classList.toggle("low", y > 0.55);
    if (pad) pad.paint();
  };
  function pickAxes() {
    const list = targets(dev);
    const col = (axis, which) => h("div", { class: "pick" }, h("h3", {}, axis),
      list.map((t) => h("button", { class: "opt", "aria-pressed": t.key === (which === "x" ? ax : ay), disabled: !bind(dev, t),
        onclick: () => {
          if (which === "x") { ax = t.key; store.set("xy.x", ax); } else { ay = t.key; store.set("xy.y", ay); }
          close();
          pickAxes();
        } }, t.name)));
    const springBtn = h("button", { class: "opt", "aria-pressed": spring, onclick: () => {
      spring = !spring;
      store.set("xy.spring", spring);
      springBtn.setAttribute("aria-pressed", spring);
      springBtn.firstChild.textContent = spring ? "Spring back: on" : "Spring back: off";
      paintRead();
    } }, h("span", {}, spring ? "Spring back: on" : "Spring back: off"), h("small", {}, "let go: back to where it was"));
    const close = sheet("XY pad", [
      springBtn,
      h("p", { class: "dim" }, "Across and up. The track's choices move the selected track (Sound tab)."),
      h("div", { class: "pick2" }, col("across", "x"), col("up", "y")),
    ]);
  }

  /* ---- fill ---- */
  let fillHeld = false;
  const fill = h("button", { class: "chip big", "aria-label": "Fill, while held" }, "fill");
  holdable(fill, () => { fillHeld = true; dev.op(PF.FILL, [1]); }, () => { fillHeld = false; dev.op(PF.FILL, [0]); });
  const fillNext = h("button", { class: "chip big", "aria-label": "Fill the next bar", onclick: () => dev.op(PF.FILL, [2]) }, "fill next");

  const el = h("section", { class: "tab live", id: "tab-live" },
    secs.el,
    trks.el,
    h("div", { class: "padwrap" },
      h("div", { class: "label" }, h("span", {}, "punch-in"), latchBtn),
      h("div", { class: "row pads" }, pads)),
    h("div", { class: "xywrap" },
      h("div", { class: "label" }, axisLab, fill, fillNext),
      xyEl));

  return {
    el,
    paint() {
      secs.paint();
      trks.paint();
      paintPads();
      paintRead();
      const s = dev.live;
      fill.classList.toggle("on", !!(s && s.fillHeld));
      fillNext.classList.toggle("arm", !!(s && s.fillNext));
      fill.disabled = fillNext.disabled = !dev.perform;
    },
    paintValues: paintRead,
    release() {                                     /* the page hidden: holds go (the device lets go too) */
      if (held.length) { held.length = 0; fxSend(); }
      if (fillHeld) { fillHeld = false; dev.op(PF.FILL, [0]); }
      paintPads();
    },
  };
}
