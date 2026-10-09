// SPDX-License-Identifier: GPL-3.0-only
// The touch controls of SLOOP live: hold buttons, faders, knob tiles, the XY pad, sheets, a toast.

export const $ = (id) => document.getElementById(id);
export function h(tag, attrs = {}, ...kids) {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) {
    if (k.startsWith("aria-") && typeof v === "boolean") { e.setAttribute(k, String(v)); continue; }   /* (aria-pressed="true", not "") */
    if (v === undefined || v === null || v === false) continue;
    if (k === "class") e.className = v;
    else if (k === "style" && typeof v === "object") Object.assign(e.style, v);
    else if (k.startsWith("--")) e.style.setProperty(k, v);
    else if (k.startsWith("on")) e.addEventListener(k.slice(2), v);
    else e.setAttribute(k, v === true ? "" : v);
  }
  for (const k of kids.flat(Infinity)) if (k !== null && k !== undefined && k !== false) e.append(k.nodeType ? k : String(k));
  return e;
}
export const buzz = (p = 8) => { try { if (navigator.vibrate) navigator.vibrate(p); } catch (_) {} };

export function toast(msg, ms = 2000) {
  const t = $("toast");
  t.textContent = msg;
  t.hidden = false;
  clearTimeout(toast.t);
  toast.t = setTimeout(() => { t.hidden = true; }, ms);
}

/* press / release for a pointer, with capture (a finger sliding off still lets go), and the keyboard */
export function holdable(el, down, up = () => {}) {
  el.addEventListener("pointerdown", (e) => { e.preventDefault(); try { el.setPointerCapture(e.pointerId); } catch (_) {} down(e); });
  const end = (e) => { try { if (el.hasPointerCapture(e.pointerId)) el.releasePointerCapture(e.pointerId); } catch (_) {} up(e); };
  el.addEventListener("pointerup", end);
  el.addEventListener("pointercancel", end);
  el.addEventListener("contextmenu", (e) => e.preventDefault());
  el.addEventListener("keydown", (e) => { if ((e.key === " " || e.key === "Enter") && !e.repeat) { e.preventDefault(); down(e); } });
  el.addEventListener("keyup", (e) => { if (e.key === " " || e.key === "Enter") up(e); });
}

/* tap, or hold (ms): onTap / onHold (fires while still down) / onHoldEnd */
export function tapHold(el, { onTap = () => {}, onHold = () => {}, onHoldEnd = () => {}, ms = 450 }) {
  let t = 0, held = false;
  holdable(el, () => {
    held = false;
    clearTimeout(t);
    t = setTimeout(() => { held = true; el.classList.add("holding"); buzz([6, 30, 6]); onHold(); }, ms);
  }, (e) => {
    clearTimeout(t);
    el.classList.remove("holding");
    if (e.type === "pointercancel") { if (held) onHoldEnd(); return; }
    if (held) onHoldEnd(); else onTap();
  });
}

/* a touch fader: drag anywhere on it (relative: a touch does not jump), double tap: its default */
export function fader(el, { min, max, get, set, label, center = false, vertical = false, fmt = (v) => v, def }) {
  el.classList.add("fader", vertical ? "v" : "h");
  if (center) el.classList.add("center");
  el.setAttribute("role", "slider");
  el.tabIndex = 0;
  el.setAttribute("aria-label", label);
  el.setAttribute("aria-valuemin", min);
  el.setAttribute("aria-valuemax", max);
  el.replaceChildren(h("div", { class: "fill" }), h("span", { class: "lab" }));
  const fill = el.firstChild, lab = el.lastChild;
  let start = null, lastTap = 0;
  const paint = () => {
    const v = get(), f = (v - min) / (max - min || 1);
    if (vertical) fill.style.height = f * 100 + "%";
    else if (center) {
      const z = (0 - min) / (max - min);
      fill.style.left = Math.min(f, z) * 100 + "%";
      fill.style.width = Math.abs(f - z) * 100 + "%";
    } else { fill.style.left = 0; fill.style.width = f * 100 + "%"; }
    lab.textContent = label ? label + " " + fmt(v) : fmt(v);
    el.setAttribute("aria-valuenow", v);
    el.setAttribute("aria-valuetext", fmt(v));
  };
  el.addEventListener("pointerdown", (e) => {
    e.preventDefault();
    el.setPointerCapture(e.pointerId);
    const now = performance.now();
    if (now - lastTap < 300) { set(center ? 0 : def ?? get()); paint(); }
    lastTap = now;
    start = { x: e.clientX, y: e.clientY, v: get() };
  });
  el.addEventListener("pointermove", (e) => {
    if (!start || !el.hasPointerCapture(e.pointerId)) return;
    const r = el.getBoundingClientRect();
    const d = vertical ? (start.y - e.clientY) / r.height : (e.clientX - start.x) / r.width;
    let v = start.v + d * (max - min);
    if (center && Math.abs(v) < (max - min) * 0.03) v = 0;
    set(Math.max(min, Math.min(max, Math.round(v))));
    paint();
  });
  const end = () => { start = null; };
  el.addEventListener("pointerup", end);
  el.addEventListener("pointercancel", end);
  el.addEventListener("keydown", (e) => {
    const step = { ArrowUp: 1, ArrowRight: 1, ArrowDown: -1, ArrowLeft: -1, PageUp: 8, PageDown: -8 }[e.key];
    if (!step) return;
    e.preventDefault();
    set(Math.max(min, Math.min(max, get() + step)));
    paint();
  });
  el.paint = paint;
  paint();
  return el;
}

/* a knob tile: the device's knob as a ring; drag up / down anywhere on the tile (relative), double tap: the
   default; an enum also opens its list on a tap (onPick) */
export function knob({ desc, label, get, set, fmt, color = "var(--fg2)", onPick }) {
  const d = desc, span = d.max - d.min || 1, bip = d.min < 0 && d.max > 0;
  const ring = h("div", { class: "ring" }, h("i", { class: "arc" }));
  const val = h("span", { class: "kv" });
  const el = h("div", { class: "knob", role: "slider", tabindex: 0, "aria-label": label, "aria-valuemin": d.min,
    "aria-valuemax": d.max, "--c": color }, ring, h("span", { class: "kl" }, label), val);
  let start = null, lastTap = 0, moved = false;
  const paint = () => {
    const v = get(), f = (v - d.min) / span, z = bip ? (0 - d.min) / span : 0;
    ring.style.setProperty("--a", Math.min(f, z) * 270 + "deg");
    ring.style.setProperty("--b", Math.max(f, z) * 270 + "deg");
    val.textContent = fmt(v);
    el.setAttribute("aria-valuenow", v);
    el.setAttribute("aria-valuetext", fmt(v));
  };
  const put = (v) => { v = Math.max(d.min, Math.min(d.max, Math.round(v))); if (v !== get()) { set(v); paint(); } };
  /* full range over ~200 px; an enum one step per ~28 px */
  const perPx = d.fmt === 8 || span <= 12 ? 1 / 28 : span / 200;
  el.addEventListener("pointerdown", (e) => {
    e.preventDefault();
    el.setPointerCapture(e.pointerId);
    start = { y: e.clientY, x: e.clientX, v: get() };
    moved = false;
    el.classList.add("active");
  });
  el.addEventListener("pointermove", (e) => {
    if (!start) return;
    const dy = start.y - e.clientY + (e.clientX - start.x) * 0.5;
    if (Math.abs(dy) > 4) moved = true;
    if (moved) put(start.v + dy * perPx);
  });
  const end = (e) => {
    if (!start) return;
    el.classList.remove("active");
    start = null;
    if (moved || e.type === "pointercancel") return;
    const now = performance.now();
    if (now - lastTap < 320) { put(d.def); buzz(6); lastTap = 0; return; }
    lastTap = now;
    if (onPick) setTimeout(() => { if (lastTap === now) onPick(); }, 330);
  };
  el.addEventListener("pointerup", end);
  el.addEventListener("pointercancel", end);
  el.addEventListener("keydown", (e) => {
    const step = { ArrowUp: 1, ArrowRight: 1, ArrowDown: -1, ArrowLeft: -1, PageUp: 8, PageDown: -8 }[e.key];
    if (step) { e.preventDefault(); put(get() + step); }
    else if (e.key === "Enter" && onPick) onPick();
  });
  el.paint = paint;
  paint();
  return el;
}

/* the XY pad: absolute (the dot goes where the finger is); onMove(x, y) with 0..1 (y up), onUp() */
export function xyPad(el, { get, onDown = () => {}, onMove, onUp = () => {} }) {
  el.classList.add("xy");
  el.setAttribute("role", "application");
  el.setAttribute("aria-label", "XY pad");
  const dot = h("i", { class: "dot" });
  el.append(h("i", { class: "hx" }), h("i", { class: "vy" }), dot);
  let id = null;
  const at = (e) => {
    const r = el.getBoundingClientRect();
    return [Math.max(0, Math.min(1, (e.clientX - r.left) / r.width)), Math.max(0, Math.min(1, 1 - (e.clientY - r.top) / r.height))];
  };
  const paint = () => {
    const [x, y] = get();
    dot.style.left = x * 100 + "%";
    dot.style.top = (1 - y) * 100 + "%";
    el.classList.toggle("touched", id !== null);
  };
  el.addEventListener("pointerdown", (e) => {
    if (id !== null) return;
    e.preventDefault();
    id = e.pointerId;
    el.setPointerCapture(id);
    onDown();
    onMove(...at(e));
    paint();
  });
  el.addEventListener("pointermove", (e) => { if (e.pointerId === id) { onMove(...at(e)); paint(); } });
  const end = (e) => { if (e.pointerId !== id) return; id = null; onUp(); paint(); };
  el.addEventListener("pointerup", end);
  el.addEventListener("pointercancel", end);
  el.paint = paint;
  paint();
  return el;
}

/* a bottom sheet: sheet(title, body) -> close(); the back, Escape or the close button close it */
export function sheet(title, body, { onClose = () => {} } = {}) {
  let gone = false;                                  /* (it slides away; reduced motion: at once) */
  const close = () => {
    if (gone) return;
    gone = true;
    document.removeEventListener("keydown", esc);
    const still = matchMedia("(prefers-reduced-motion: reduce)").matches;
    if (still) back.remove(); else { back.classList.add("closing"); setTimeout(() => back.remove(), 200); }
    onClose();
  };
  const esc = (e) => { if (e.key === "Escape") close(); };
  const box = h("div", { class: "sheet", role: "dialog", "aria-modal": "true", "aria-label": title },
    h("header", {}, h("h2", {}, title), h("button", { class: "x", onclick: () => close(), "aria-label": "Close" }, "close")),
    h("div", { class: "body" }, body));
  const back = h("div", { class: "sheet-back", onclick: (e) => { if (e.target === back) close(); } }, box);
  document.body.append(back);
  document.addEventListener("keydown", esc);
  const f = box.querySelector(".body button, .body input");
  if (f) f.focus({ preventScroll: true });
  return close;
}

/* a "next step" card: what it would do, and the decision it needs first */
export function nextCard({ title, what, decide }) {
  return h("article", { class: "next" }, h("h3", {}, title), h("p", {}, what), h("p", { class: "decide" }, decide));
}
