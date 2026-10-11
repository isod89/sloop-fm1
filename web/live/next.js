// SPDX-License-Identifier: GPL-3.0-only
// What SLOOP live could do next, by tab: what each would do, and the decision it needs before building.
export const NEXT = {
  arrange: [
    { title: "Edit the song order",
      what: "See the song as a list of sections with their bars, reorder them, change a repeat, without playing it in again.",
      decide: "Needs: reading and writing the song order over USB. Today it only travels inside a full backup. Decide between a new PERFORM op and reading the backup's settings object." },
    { title: "Clear a section",
      what: "Empty a section you no longer want, from its menu.",
      decide: "Needs: a firmware op. Decide whether it can be undone, since the device's undo covers patterns, not sections." },
    { title: "Repeats in a chain",
      what: "A chain step that plays its section twice, or for 8 bars, like A A B C.",
      decide: "Needs: the firmware chain to keep a bar count per step (now each step plays its section's own length)." },
    { title: "Steps on the phone",
      what: "See and edit the pattern: a 16-lane drum grid with levels and ratchets, and the synth steps.",
      decide: "The protocol already reads and writes every step. It is a big screen of its own: decide whether it belongs here or in the web editor." },
  ],
  sound: [
    { title: "Mutate",
      what: "One button nudges the sound a little in a random direction; another takes the last nudge back.",
      decide: "Decide which parameters are safe to move (levels, tuning and voices can jump) and how far each may go." },
    { title: "Record knob moves",
      what: "Turn a knob while the loop plays and the pattern keeps the move, as parameter locks on the steps.",
      decide: "The firmware has locks (24 a track). Decide the overdub rules (replace or add, which steps) and how to show them." },
    { title: "Shape the SYN drum kits",
      what: "The synthesised drum sounds, lane by lane: pitch, decay, noise, click.",
      decide: "The protocol has it (DSYN). Decide how much of the 22 values a sound needs on a phone." },
  ],
  mix: [
    { title: "Scenes and morph",
      what: "Snapshot the mix and the sounds, recall one on a tap, or slide between two with a crossfader.",
      decide: "Decide what a scene holds (mix only, or sounds too) and how fast the phone can stream changes over USB without crowding the pads." },
  ],
  setup: [
    { title: "Play without the cable",
      what: "Leave the FM-1 on the Mac or the jam-room Pi and play from any phone, iPhones too.",
      decide: "The page already talks to a WebSocket relay (?ws=). The relay is the missing piece. Bluetooth MIDI needs the firmware to turn its radio on." },
  ],
};
