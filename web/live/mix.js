// SPDX-License-Identifier: GPL-3.0-only
// MIX: the four levels with mute and solo, and the master: tempo, swing, dust, duck, roll rate, the DJ filter.
import { PF, fmtValue } from "./proto.js";
import { h, fader } from "./widgets.js";
import { TC, trackName } from "./parts.js";

export function mixTab(dev) {
  const faders = [];
  const strips = [0, 1, 2, 3].map((i) => {
    const f = h("div", { "--c": TC[i] });
    faders.push(fader(f, { min: 0, max: 127, vertical: true, label: "", get: () => (dev.tracks[i] ? dev.tracks[i].level : 0),
      set: (v) => dev.setLevel(i, v) }));
    const m = h("button", { class: "chip", onclick: () => {
      if (dev.perform) dev.op(PF.MUTE, [i, 2]);
      else { const t = dev.tracks[i]; t.mute = !t.mute; dev.setLevel(i, t.level); paint(); }
    } }, "mute");
    const s = h("button", { class: "chip", onclick: () => dev.op(PF.SOLO, [i, 2]) }, "solo");
    return { el: h("div", { class: "chan", "--c": TC[i] }, f, h("div", { class: "name" }, h("b", {}, String(i + 1)), h("span", {})),
      h("div", { class: "row two" }, m, s)), m, s };
  });
  const master = h("div", { class: "row master" });
  const el = h("section", { class: "tab mix", id: "tab-mix", hidden: true },
    h("div", { class: "row mixer" }, strips.map((s) => s.el)),
    h("h2", {}, "Master"), master);

  function build() {
    master.replaceChildren();
    const g = (label, name, c, opt = {}) => {
      const p = dev.g[label];
      if (!p) return;
      const e = h("div", { "--c": c });
      master.append(e);
      faders.push(fader(e, { min: p.desc.min, max: p.desc.max, def: p.desc.def, label: name, get: () => p.value,
        set: (v) => dev.setGlobal(label, v), fmt: (v) => fmtValue(p.desc, v).replace(" bpm", ""), ...opt }));
    };
    faders.length = 4;
    g("BPM", "tempo", "var(--fg2)");
    g("SWING", "swing", "var(--t2)");
    g("DUST", "dust", "var(--t3)");
    g("DUCK", "duck", "var(--t4)");
    g("ROLL", "roll rate", "var(--t1)");
    g("FILT", "filter", "var(--fg2)", { center: true });
  }
  function paint() {
    strips.forEach((s, i) => {
      const t = dev.tracks[i];
      if (!t) return;
      s.el.querySelector(".name span").textContent = trackName(dev, i);
      s.m.classList.toggle("on", t.mute);
      s.s.classList.toggle("on", !!(dev.solo >> i & 1));
      s.s.disabled = !dev.perform;
    });
    for (const f of faders) if (f) f.paint();
  }
  return { el, build, paint };
}
