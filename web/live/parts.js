// SPDX-License-Identifier: GPL-3.0-only
// Pieces more than one tab shows: the section tiles (A..D) and the track tiles (1..4).
import { PF } from "./proto.js";
import { h, tapHold, toast, buzz } from "./widgets.js";

export const TC = ["var(--t1)", "var(--t2)", "var(--t3)", "var(--t4)"];
export const SEC = "ABCD";

/* the name a track goes by: its preset (or engine), "drums" */
export function trackName(dev, i) {
  const t = dev.tracks[i];
  if (!t) return "";
  if (dev.isDrum(i)) return "drums";
  const n = dev.names && dev.names[t.eng];
  return ((n && n.names[t.preset]) || (dev.info.engines[t.eng] || "")).toLowerCase();
}

/* where the playing section is: bar (1..), beat (1..4), of its bars; null when stopped */
export function position(s) {
  if (!s || !s.playing) return null;
  const bars = s.bars && s.section >= 0 ? s.bars[s.section] || 0 : 0;
  const bar = bars ? Math.floor(s.beat / 4) % bars : Math.floor(s.beat / 4);
  return { bar: bar + 1, beat: (s.beat % 4) + 1, bars, frac: bars ? (bar * 4 + (s.beat % 4) + 1) / (bars * 4) : 0 };
}

/* store the loop into a section: a used one asks twice on the device, so the page asks too */
let armed = -1, armT = 0;
export async function storeSection(dev, s, { confirmed = false } = {}) {
  const used = !!(dev.live && dev.live.ready >> s & 1);
  if (used && !confirmed && !(armed === s && performance.now() - armT < 3000)) {
    armed = s; armT = performance.now();
    await dev.op(PF.STORE, [s]);                       /* (the device arms too) */
    toast(`Hold ${SEC[s]} again to replace it`, 2600);
    return false;
  }
  if (used && confirmed) await dev.op(PF.STORE, [s]);  /* (arm, then store) */
  armed = -1;
  await dev.op(PF.STORE, [s]);
  buzz([10, 40, 10]);
  toast(`Saved the loop into ${SEC[s]}`);
  return true;
}

export async function launchSection(dev, s) {
  const st = await dev.op(PF.SECTION, [s]);
  if (st && !(st.ready >> s & 1)) toast(`${SEC[s]} is empty. Hold it to save the loop into it.`, 2600);
  else if (st && st.songPlays) toast("The song is playing. Stop it, or turn song mode off.");
}

/* section tiles: tap plays (stopped: loads; playing: the next bar), hold saves the loop (or onHold) */
export function sectionTiles(dev, { big = false, onHold, onTap } = {}) {
  const tiles = [0, 1, 2, 3].map((s) => {
    const b = h("button", { class: "sec" + (big ? " big" : ""), "--c": TC[s], "aria-label": "Section " + SEC[s] },
      h("b", {}, SEC[s]), h("small", {}), h("i", { class: "prog" }));
    tapHold(b, { onTap: () => (onTap ? onTap(s) : launchSection(dev, s)), onHold: () => (onHold ? onHold(s) : storeSection(dev, s)) });
    return b;
  });
  const paint = () => {
    const st = dev.live, pos = position(st);
    tiles.forEach((b, i) => {
      const ready = !!(st && st.ready >> i & 1), bars = st && st.bars ? st.bars[i] : 0;
      b.classList.toggle("empty", !ready);
      b.classList.toggle("cur", !!st && st.section === i && ready);
      b.classList.toggle("next", !!st && st.next === i);
      b.disabled = !dev.perform;
      const small = b.querySelector("small");
      if (big) small.textContent = !ready ? "empty" : st.next === i ? "starts next bar"
        : st.section === i && pos && bars ? `bar ${pos.bar} of ${bars}` : bars ? bars + (bars === 1 ? " bar" : " bars") : "";
      else small.textContent = ready && bars ? String(bars) : "";
      b.querySelector(".prog").style.width = st && st.section === i && pos && pos.bars ? pos.frac * 100 + "%" : "0";
    });
  };
  return { el: h("div", { class: "row sections" + (big ? " big" : "") }, tiles), paint };
}

/* track tiles: tap mutes, hold solos while held (the others fade, as GLO + key does) */
export function trackTiles(dev) {
  const tiles = [0, 1, 2, 3].map((i) => {
    const b = h("button", { class: "trk", "--c": TC[i], "aria-label": `Track ${i + 1}: tap to mute, hold to solo` },
      h("b", {}, String(i + 1)), h("span", {}));
    let wasSolo = false;
    tapHold(b, {
      ms: 300,
      onTap: () => {
        if (dev.perform) { dev.op(PF.MUTE, [i, 2]); return; }
        const t = dev.tracks[i];                         /* (older firmware: the mute through TRACK_MIX) */
        t.mute = !t.mute;
        dev.setLevel(i, t.level);
        paint();
      },
      onHold: () => { wasSolo = !!(dev.solo >> i & 1); if (!wasSolo) dev.op(PF.SOLO, [i, 1]); },
      onHoldEnd: () => { if (!wasSolo) dev.op(PF.SOLO, [i, 0]); },
    });
    return b;
  });
  const paint = () => {
    tiles.forEach((b, i) => {
      const t = dev.tracks[i];
      if (!t) return;
      const solo = !!(dev.solo >> i & 1);
      b.classList.toggle("muted", t.mute);
      b.classList.toggle("solo", solo);
      b.classList.toggle("quiet", !!dev.solo && !solo);
      b.classList.toggle("sel", dev.sel === i);
      b.querySelector("span").textContent = t.mute ? "muted" : solo ? "solo" : trackName(dev, i);
      b.setAttribute("aria-pressed", t.mute);
    });
  };
  return { el: h("div", { class: "row tracks" }, tiles), paint };
}
