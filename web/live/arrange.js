// SPDX-License-Identifier: GPL-3.0-only
// ARRANGE: from a loop to a song. Capture the loop into the next empty section, launch sections, build a chain,
// record the song as you play it.
import { PF } from "./proto.js";
import { h, sheet, toast } from "./widgets.js";
import { TC, SEC, sectionTiles, storeSection, launchSection } from "./parts.js";

export function arrangeTab(dev) {
  let picking = false;                               /* all four used: the next tap picks the one to replace */
  const secs = sectionTiles(dev, { big: true, onHold: (s) => sectionMenu(s), onTap: (s) => {
    if (!picking) { launchSection(dev, s); return; }
    picking = false;                                 /* (pick mode: the tap replaces it) */
    storeSection(dev, s, { confirmed: true });
    paint();
  } });

  const nextEmpty = () => { const r = dev.live ? dev.live.ready : 15; for (let s = 0; s < 4; s++) if (!(r >> s & 1)) return s; return -1; };
  const capture = h("button", { class: "capture", onclick: async () => {
    const s = nextEmpty();
    if (picking) { picking = false; paint(); return; }
    if (s < 0) { picking = true; paint(); toast("Tap the section to replace"); return; }
    await storeSection(dev, s);
  } }, h("b", {}), h("span", {}));

  function sectionMenu(s) {
    const used = !!(dev.live && dev.live.ready >> s & 1);
    const close = sheet("Section " + SEC[s], [
      h("button", { class: "opt", disabled: !used, onclick: () => { close(); launchSection(dev, s); } }, used ? "Play it" : "Empty"),
      h("button", { class: "opt", onclick: () => { close(); storeSection(dev, s, { confirmed: true }); } },
        used ? "Replace it with the loop that is playing" : "Save the loop into it"),
      h("button", { class: "opt", disabled: !used, onclick: () => { close(); chain.push(s); paintChain(); } }, "Add it to the chain"),
      h("button", { class: "opt", disabled: true }, "Clear it (next step: needs a firmware op)"),
    ]);
  }

  /* ---- the chain: tap A..D to add, a chip to take it out ---- */
  const chain = [];
  const chips = h("div", { class: "chips", "aria-live": "polite" });
  const adds = [0, 1, 2, 3].map((s) => h("button", { class: "add", "--c": TC[s], "aria-label": "Add " + SEC[s] + " to the chain",
    onclick: () => { if (chain.length < 8) { chain.push(s); paintChain(); } } }, "+" + SEC[s]));
  const playChain = h("button", { class: "chip big pri", onclick: async () => {
    if (!chain.length) return;
    const st = await dev.op(PF.CHAIN, [chain.length, ...chain]);
    if (!st) return;
    if (st.rc === 1) toast("Press play first: a chain starts on the next bar.");
    else if (st.rc === 2) toast("A section in the chain is empty.");
    else if (st.rc === 3) toast("The song is playing. Stop it first.");
  } }, "play chain");
  const clearChain = h("button", { class: "chip big", onclick: () => { chain.length = 0; paintChain(); } }, "clear");
  const chainInfo = h("p", { class: "dim" });
  const paintChain = () => {
    const st = dev.live, playing = st && st.chain.length ? st.chain : null;
    chips.replaceChildren(...(chain.length ? chain : []).map((s, i) => h("button", { class: "chipsec", "--c": TC[s],
      "aria-label": `Remove ${SEC[s]} at ${i + 1}`, onclick: () => { chain.splice(i, 1); paintChain(); } }, SEC[s])));
    if (!chain.length) chips.append(h("span", { class: "dim" }, "Tap +A +B +C +D in the order to play them."));
    const bars = st && st.bars ? chain.reduce((n, s) => n + (st.bars[s] || 0), 0) : 0;
    chainInfo.textContent = playing
      ? "Playing: " + playing.map((s, i) => (i === st.chainI ? `[${SEC[s]}]` : SEC[s])).join(" ") + ", looped. A single section ends it."
      : chain.length && bars ? `${chain.length} steps, ${bars} bars, looped. It starts on the next bar.` : "";
    adds.forEach((b, s) => { b.disabled = !(st && st.ready >> s & 1) || chain.length >= 8; });
    playChain.disabled = !chain.length || !dev.perform;
  };

  /* ---- the song ---- */
  const songMode = h("button", { class: "chip big", onclick: () => dev.op(PF.SONGMODE, [2]) }, "song mode");
  const songRec = h("button", { class: "chip big", onclick: () => dev.op(PF.SONGREC) }, "record the song");
  const songInfo = h("p", { class: "dim" });

  const el = h("section", { class: "tab arrange", id: "tab-arrange", hidden: true },
    capture,
    secs.el,
    h("div", { class: "card" }, h("h2", {}, "Chain"), chips, h("div", { class: "row adds" }, adds),
      h("div", { class: "row two" }, playChain, clearChain), chainInfo),
    h("div", { class: "card" }, h("h2", {}, "Song"),
      h("div", { class: "row two" }, songRec, songMode), songInfo));

  function paint() {
    const st = dev.live, s = nextEmpty();
    secs.paint();
    secs.el.classList.toggle("picking", picking);
    capture.disabled = !dev.perform;
    capture.querySelector("b").textContent = picking ? "Cancel" : s >= 0 ? `Capture into ${SEC[s]}` : "Replace a section";
    capture.querySelector("span").textContent = picking ? "Tap the section the loop replaces"
      : s >= 0 ? "Saves the loop that is playing, to come back to" : "All four sections hold a loop";
    capture.style.setProperty("--c", s >= 0 ? TC[s] : "var(--fg2)");
    paintChain();
    if (!st) return;
    songMode.classList.toggle("on", st.songMode);
    songMode.disabled = songRec.disabled = !dev.perform;
    songRec.classList.toggle("on", st.songrec === 2);
    songRec.classList.toggle("arm", st.songrec === 1);
    songRec.textContent = st.songrec === 2 ? "stop recording" : st.songrec === 1 ? "recording from the next bar" : "record the song";
    songInfo.textContent = st.songrec ? "Launch sections as the song should go. Press stop recording (or STOP) to keep it."
      : st.songPlays ? "The song is playing."
      : st.songMode ? "Song mode: PLAY plays the recorded song." : "Record: launch sections in order, the device writes it down. Song mode plays it back.";
  }
  return { el, paint };
}
