// SPDX-License-Identifier: GPL-3.0-only
// SOUND: the selected track's sound, no menus. Flip presets (or kits), turn the engine's eight knobs, the
// envelope, the sends, the LFO, the arp; revert to the preset; keep a sound as a user preset. The selected track
// is the FM-1's: pick one here and the device follows, pick one there and this page follows.
import { fmtValue } from "./proto.js";
import { TP_IDS } from "./device.js";
import { h, knob, sheet, toast } from "./widgets.js";
import { TC, trackName } from "./parts.js";

const T = TP_IDS;
const PAGES = [
  { key: "sound", name: "sound" },                   /* the engine's own eight (P_E0..P_E7), titled as on the device */
  { key: "shape", name: "shape", ids: [[T.ATK, "attack"], [T.DEC, "decay"], [T.SUS, "sustain"], [T.REL, "release"],
    [T.TFLT, "filter"], [T.EFLT, "env > filter"], [T.EPIT, "env > pitch"], [T.GLD, "glide"]] },
  { key: "fx", name: "fx", ids: [[T.DST, "drive"], [T.CHO, "chorus"], [T.DLY, "delay"], [T.REV, "reverb"],
    [T.SLCR, "slicer"], [T.SLPAT, "pattern"], [T.SLRATE, "slice rate"], [T.SLDEPTH, "depth"]] },
  { key: "lfo", name: "lfo", ids: [[T.LRATE, "rate"], [T.LWAVE, "wave"], [T.LFADE, "fade in"], [T.LPHS, "phase"],
    [T.LPIT, "> pitch"], [T.LFLT, "> filter"], [T.LSHP, "> shape"], [T.LAMP, "> level"]] },
  { key: "arp", name: "arp", ids: [[T.AMODE, "mode"], [T.ARATE, "rate"], [T.AOCT, "octaves"], [T.AGATE, "gate"],
    [T.ASWG, "swing"], [T.APROB, "chance"], [T.AHOLD, "hold"], [T.AORD, "order"]] },
];

export function soundTab(dev) {
  let page = "sound", userName = null;
  const tabs = [0, 1, 2, 3].map((i) => h("button", { class: "ttab", "--c": TC[i], "aria-label": "Track " + (i + 1),
    onclick: () => { userName = null; dev.selectTrack(i); } }, h("b", {}, String(i + 1)), h("span", {})));
  const eng = h("button", { class: "engname", onclick: () => engineSheet() });
  const name = h("button", { class: "pname", onclick: () => (dev.sound && dev.sound.drum ? kitSheet() : presetSheet()) });
  const prev = h("button", { class: "step", "aria-label": "Previous", onclick: () => stepPreset(-1) }, "‹");
  const next = h("button", { class: "step", "aria-label": "Next", onclick: () => stepPreset(1) }, "›");
  const revert = h("button", { class: "chip", onclick: async () => {
    const s = dev.sound;
    if (!s || s.drum) return;
    userName = null;
    await dev.preset(s.eng, s.preset);
    toast("Back to the preset");
  } }, "revert");
  const save = h("button", { class: "chip", onclick: () => saveSheet() }, "keep");
  const pageBar = h("div", { class: "seg", role: "tablist" });
  const grid = h("div", { class: "knobs" });
  const el = h("section", { class: "tab sound", id: "tab-sound", hidden: true },
    h("div", { class: "row ttabs" }, tabs),
    h("div", { class: "pbar" }, prev, h("div", { class: "pmid" }, eng, name), next),
    h("div", { class: "row acts" }, revert, save),
    pageBar, grid);

  let knobs = [];
  function build() {
    const s = dev.sound;
    grid.replaceChildren();
    knobs = [];
    if (!s) { grid.append(h("p", { class: "dim wide" }, "Reading the sound...")); return; }
    const color = TC[s.track];
    const mk = (id, label) => {
      const d = dev.desc(id);
      if (!d || !d.label || d.label === "-") return h("div", { class: "knob empty" });
      const k = knob({ desc: d, label, color, get: () => dev.sound.p[id], set: (v) => dev.setParam(id, v), fmt: (v) => fmtValue(d, v),
        onPick: d.fmt === 8 ? () => enumSheet(id, label, d) : undefined });
      k.dataset.id = id;
      knobs.push(k);
      return k;
    };
    if (s.drum) {
      pageBar.hidden = true;
      const g = (label, nm) => {
        const p = dev.g[label];
        if (!p) return h("div", { class: "knob empty" });
        const k = knob({ desc: p.desc, label: nm, color, get: () => p.value, set: (v) => dev.setGlobal(label, v), fmt: (v) => fmtValue(p.desc, v) });
        k.dataset.g = label;
        knobs.push(k);
        return k;
      };
      grid.append(g("LVL", "level"), g("REV", "reverb"), g("DLY", "delay"), mk(T.SSWG, "swing"), mk(T.PAN, "pan"), mk(T.TFLT, "filter"));
      return;
    }
    pageBar.hidden = false;
    const titles = (dev.names[s.eng] && dev.names[s.eng].pages) || ["", ""];
    pageBar.replaceChildren(...PAGES.map((p) => h("button", { role: "tab", "aria-selected": p.key === page,
      onclick: () => { page = p.key; build(); } }, p.key === "sound" ? (dev.info.engines[s.eng] || "sound").toLowerCase() : p.name)));
    if (page === "sound") {
      for (let row = 0; row < 2; row++) {
        grid.append(h("h3", { class: "rowt" }, (titles[row] || "").toLowerCase()));
        for (let k = 0; k < 4; k++) {
          const id = dev.info.pE0 + row * 4 + k, d = dev.desc(id);
          grid.append(mk(id, d ? d.label.toLowerCase() : ""));
        }
      }
    } else {
      for (const [id, label] of PAGES.find((p) => p.key === page).ids) grid.append(mk(id, label));
    }
  }

  async function stepPreset(dir) {
    const s = dev.sound;
    if (!s) return;
    userName = null;
    if (s.drum) {
      const d = dev.desc(dev.info.pE0);
      if (!d) return;
      const n = d.max - d.min + 1, v = ((s.p[dev.info.pE0] - d.min + dir) % n + n) % n + d.min;
      dev.setParam(dev.info.pE0, v);
      paint();
      return;
    }
    const names = (await dev.engineNames(s.eng)).names;
    await dev.preset(s.eng, (s.preset + dir + names.length) % names.length);
  }

  function presetSheet() {
    const s = dev.sound;
    const names = (dev.names[s.eng] || { names: [] }).names;
    const mine = h("div", { class: "list" }, h("p", { class: "dim" }, "Reading your sounds..."));
    const close = sheet((dev.info.engines[s.eng] || "").toLowerCase() + " presets", [
      h("div", { class: "list" }, names.map((n, i) => h("button", { class: "opt", "aria-pressed": i === s.preset && !userName,
        onclick: async () => { close(); userName = null; await dev.preset(s.eng, i); } }, n.toLowerCase()))),
      h("h3", {}, "Your sounds"), mine,
    ]);
    dev.userPresets().then((ups) => {
      const used = ups.filter((u) => u.used);
      mine.replaceChildren(...(used.length ? used.map((u) => h("button", { class: "opt", onclick: async () => {
        close();
        const rc = await dev.loadUser(u.slot);
        if (rc) toast("That slot is empty."); else { userName = u.name; paint(); }
      } }, h("span", {}, u.name.toLowerCase()), h("small", {}, (dev.info.engines[u.eng] || "").toLowerCase())))
        : [h("p", { class: "dim" }, "None yet. Keep a sound with the keep button.")]));
    }).catch(() => mine.replaceChildren(h("p", { class: "dim" }, "Could not read them.")));
  }

  function kitSheet() {
    const s = dev.sound, d = dev.desc(dev.info.pE0);
    if (!d) return;
    const close = sheet("Drum kits", h("div", { class: "list grid2" }, d.names.map((n, i) => h("button", { class: "opt",
      "aria-pressed": s.p[dev.info.pE0] === d.min + i, onclick: () => { close(); dev.setParam(dev.info.pE0, d.min + i); paint(); } }, n.toLowerCase()))));
  }

  function engineSheet() {
    const s = dev.sound;
    if (!s || s.drum) return;
    const close = sheet("Engine", [
      h("p", { class: "dim" }, "A new engine starts from its first preset. The pattern, the mix and the key stay."),
      h("div", { class: "list grid2" }, dev.info.engines.map((n, i) => h("button", { class: "opt", "aria-pressed": i === s.eng,
        onclick: async () => { close(); userName = null; await dev.preset(i, 0); } }, n.toLowerCase()))),
    ]);
  }

  function enumSheet(id, label, d) {
    const close = sheet(label, h("div", { class: "list grid2" }, d.names.map((n, i) => h("button", { class: "opt",
      "aria-pressed": dev.sound.p[id] === d.min + i, onclick: () => { close(); dev.setParam(id, d.min + i); paintValues(); } }, n.toLowerCase()))));
  }

  function saveSheet() {
    const s = dev.sound;
    if (!s || s.drum) { toast("Drum kits are kept with the project."); return; }
    const input = h("input", { type: "text", id: "upname", maxlength: 12, autocomplete: "off", spellcheck: "false",
      value: ((dev.names[s.eng] && dev.names[s.eng].names[s.preset]) || "").slice(0, 9) + " 2" });
    const msg = h("p", { class: "dim" }, "Up to 12 letters. It goes into the first free user slot (U01..U32).");
    const go = h("button", { class: "chip big pri", onclick: async () => {
      const nm = input.value.toUpperCase().replace(/[^ -~]/g, "").trim().slice(0, 12);
      if (!nm) { msg.textContent = "Give it a name first."; return; }
      const ups = await dev.userPresets();
      const free = ups.find((u) => !u.used);
      if (!free) { msg.textContent = "All 32 user slots are taken. Free one on the FM-1 (SAVE > USER)."; return; }
      const rc = await dev.storeUser(free.slot, nm);
      if (rc === 3) { msg.textContent = "Stop the song first: keeping a sound writes the FM-1's flash."; return; }
      if (rc) { msg.textContent = "The FM-1 could not keep it (code " + rc + ")."; return; }
      close();
      userName = nm;
      paint();
      toast(`Kept as U${String(free.slot + 1).padStart(2, "0")} ${nm.toLowerCase()}`);
    } }, "keep it");
    const close = sheet("Keep this sound", [h("label", { for: "upname" }, "Name"), input, msg, go]);
  }

  function paintValues(detail) {
    for (const k of knobs) if (!detail || detail.id === undefined || +k.dataset.id === detail.id) k.paint();
  }
  function paint() {
    const s = dev.sound;
    tabs.forEach((b, i) => {
      b.setAttribute("aria-selected", dev.sel === i);
      b.classList.toggle("on", dev.sel === i);
      b.querySelector("span").textContent = dev.tracks[i] ? trackName(dev, i) : "";
    });
    if (!s) { eng.textContent = ""; name.textContent = "..."; return; }
    if (s.drum) {
      const d = dev.desc(dev.info.pE0);
      eng.textContent = "drum kit";
      name.textContent = d ? (d.names[s.p[dev.info.pE0] - d.min] || "").toLowerCase() : "";
      revert.hidden = save.hidden = true;
    } else {
      eng.textContent = (dev.info.engines[s.eng] || "").toLowerCase() + "  ▾";
      name.textContent = userName ? userName.toLowerCase() : ((dev.names[s.eng] && dev.names[s.eng].names[s.preset]) || "preset " + (s.preset + 1)).toLowerCase();
      revert.hidden = save.hidden = false;
    }
    paintValues();
  }
  return {
    el,
    rebuild() { build(); paint(); },
    paint,
    paintValues,
  };
}
