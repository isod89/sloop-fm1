// SPDX-License-Identifier: GPL-3.0-only
// HELP: the ? in the top bar. How the tab on screen is played (gestures), the full guide, and what could come next
// (folded away). The tabs themselves carry only their controls.
import { h, sheet } from "./widgets.js";
import { NEXT } from "./next.js";

export const GUIDE_URL = "https://github.com/isod89/sloop-fm1/blob/main/LIVE.md";

const HOW = {
  live: [
    ["Sections A–D", "Tap: play it on the next bar (stopped: it becomes the loop). Hold: save the loop that is playing into it; over a used one, hold it twice."],
    ["Tracks 1–4", "Tap: mute. Hold: solo while held."],
    ["Punch-in pads", "Hold for the effect, let go and it lets go. Two fingers: the last one plays. latch: a tap turns it on, another off. A framed pad is played on the FM-1's own keys."],
    ["XY pad", "Finger down and move: across the DJ filter, up DUST. It springs back when you let go. Tap the label above it to choose what each axis moves, or to make it stay."],
    ["fill", "Hold: a fill while held. fill next: the next bar is a fill."],
    ["Top bar", "Play / stop. Tap the tempo in time to set it. Tap the status for the connection and the demo."],
  ],
  arrange: [
    ["Capture", "Saves the loop that is playing into the next empty section. All four used: tap it, then the section to replace."],
    ["Sections", "Tap: play on the next bar. Hold: its menu (play, replace, add to the chain)."],
    ["Chain", "Tap +A +B +C +D in order, then play chain. Each section plays its own length, looped. Tap a block to take it out."],
    ["Song", "record the song, launch sections in order, stop recording to keep it. song mode: PLAY plays the song."],
  ],
  sound: [
    ["Track", "The FM-1's selected track: pick one here and the FM-1 follows, and the other way round."],
    ["Presets", "‹ › the previous / next preset. Tap the name for the list and your user presets. engine ▾ picks an engine."],
    ["Knobs", "Drag up or down anywhere on a tile. Double tap: the default. Named values (waves, modes): tap for the list."],
    ["revert / keep", "revert: back to the preset. keep: save it as a user preset (stop the song first if it asks)."],
  ],
  mix: [
    ["Faders", "Drag anywhere on a fader (a touch does not jump it). Double tap: the default."],
    ["mute / solo", "Under each track. Solo fades the others."],
  ],
};
const NAMES = { live: "Live", arrange: "Arrange", sound: "Sound", mix: "Mix" };

/* the help sheet, opened at the tab on screen (on a tablet: Live and the other pane) */
export function helpSheet(tabsShown) {
  const how = tabsShown.map((t) => h("section", { class: "how" }, h("h3", {}, NAMES[t]),
    h("dl", {}, HOW[t].flatMap(([k, v]) => [h("dt", {}, k), h("dd", {}, v)]))));
  const next = Object.entries({ Arrange: NEXT.arrange, Sound: NEXT.sound, Mix: NEXT.mix, Setup: NEXT.setup })
    .flatMap(([tab, items]) => items.map((n) => h("details", { class: "nextd" },
      h("summary", {}, n.title, h("small", {}, tab.toLowerCase())), h("p", {}, n.what), h("p", { class: "dim" }, n.decide))));
  sheet("Help", [
    ...how,
    h("a", { class: "chip big guide", href: GUIDE_URL, target: "_blank", rel: "noopener" }, "Open the user guide"),
    h("h3", { class: "nexth" }, "What could come next"),
    h("p", { class: "dim" }, "Ideas that need a decision (or a firmware change) first. Tap one for what it would do and what it needs."),
    ...next,
  ]);
}
