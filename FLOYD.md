# FLOYD: 4-Operator Visual FM Synthesizer Engine

The **FLOYD** engine is an advanced 4-operator FM (Frequency Modulation) synthesizer engineered for the SLOOP firmware on the M-VAVE FM-1 hardware. It pairs deep per-operator dynamic modulation controls with an intuitive 7-page vector UI and real-time oscilloscope visualization.

![Floyd 4-Operator FM Screens](assets/screens/floyd-screens.png)

---

## 1. Heritage, Inspiration & Credits

* **Concept & Architecture:** Based on the 4-operator FM synthesizer UI concept and design philosophy by **Floyd Steinberg** — [Watch the original concept video on YouTube](https://www.youtube.com/watch?v=EaxKaxi4ZuE).
* **Reference Implementation:** Inspired by **Jonathan Zeppa's** open-source VST3/AU/LV2 plugin [FloydFM on GitHub](https://github.com/JonathanZeppa/FloydFM) (MIT License).
* **SLOOP Engine & Vector UI:** Fixed-point DSP synthesis engine, 7-page sub-navigation system, and real-time vector-locked ADSR visualization developed for the M-VAVE FM-1 platform.

---

## 2. Synthesis Architecture

Unlike traditional basic FM engines with shared envelopes, **FLOYD** gives independent dynamic control to each of its four sine wave operators ($O_1 \dots O_4$):

* **4 Operators:** 
  * $O_1$: Carrier operator.
  * $O_2, O_3, O_4$: Modulator operators (with feedback loop on $O_4$).
* **Per-Operator ADSR Envelopes:** Independent Attack, Decay, Sustain, and Release envelopes for every operator.
* **Harmonic Frequency Ratios:** 15 ratio multipliers per operator (`0.5x`, `1.0x`, `2.0x`, `3.0x`, `4.0x`, `5.0x`, `6.0x`, `7.0x`, `8.0x`, `9.0x`, `10.0x`, `11.0x`, `12.0x`, `14.0x`, `16.0x`).
* **Fixed-Point DSP Core:** Implemented in pure 32-bit/16-bit integer fixed-point math (`mulq15`, `mulq16`, Q24 envelope state) optimized for the FM-1 audio interrupt handler.

---

## 3. Algorithms

FLOYD includes 8 classic FM routing topologies:

| # | Name | Routing Structure | Sonic Character |
|---|---|---|---|
| **0** | **STACK** | $4 \rightarrow 3 \rightarrow 2 \rightarrow 1$ | Rich serial cascading FM, complex spectra, metallic grit |
| **1** | **(3+4)>2>1** | $(3 + 4) \rightarrow 2 \rightarrow 1$ | Dual-modulator branch feeding serial carrier |
| **2** | **(2+4)>1** | $(2 + 4) \rightarrow 1, \; 3 \rightarrow 2$ | Parallel branches into primary carrier |
| **3** | **(2+3)>1** | $(2 + 3) \rightarrow 1, \; 4 \rightarrow 3$ | Dual modulator pair into carrier |
| **4** | **DUAL** | $2 \rightarrow 1, \; 4 \rightarrow 3$ | Two independent 2-OP FM synth voices mixed in parallel |
| **5** | **3-TO-1 (Fan)** | $(4 + 3 + 2) \rightarrow 1$ | Floyd Steinberg signature default: 3 parallel modulators into carrier (electric pianos, tines) |
| **6** | **4>3, 1, 2** | $4 \rightarrow 3, \; 1, \; 2$ | Split carrier/modulator cluster with independent sine carriers |
| **7** | **ORGAN** | $1 + 2 + 3 + 4 \rightarrow \text{Out}$ | Pure 4-drawbar additive synthesis mixing 4 independent sine harmonics |

---

## 4. 7-Page UI Sub-Navigation

When the FLOYD engine is selected, the FM-1 display provides dedicated sub-pages for granular editing:

1. **`ALGO (1/7)`**: Select routing algorithm ($0 \dots 7$) and operator feedback intensity.
2. **`OP1 (2/7)`**: Carrier ADSR envelope (Attack, Decay, Sustain, Release).
3. **`OP2 (3/7)`**: Modulator 2 ADSR envelope.
4. **`OP3 (4/7)`**: Modulator 3 ADSR envelope.
5. **`OP4 (5/7)`**: Modulator 4 ADSR envelope.
6. **`LEVEL (6/7)`**: Individual output amplitude levels for each operator ($L_1 \dots L_4$).
7. **`RATIO (7/7)`**: Harmonic frequency ratios for modulators ($R_2, R_3, R_4$) and feedback depth.

---

## 5. Live Vector Visualization

The FLOYD UI incorporates real-time vector graphics that respond dynamically during performance:

* **Real-time ADSR Vector Curves:** Visualizes the actual mathematical curve shape for the active operator's envelope.
* **Carrier / Modulator Badging:** Dynamic visual indicators distinguishing carrier operators from modulators.
* **Vector-Locked Playhead Tracers:** High-speed geometric ray-projection that tracks active voice note-on/decay/release playback, smoothly animating tracer dots along the diagonal vector segments without jitter or artifacting.

---

## 6. Factory Sound Presets

* **`FLOYD TINE`**: Classic dynamic FM electric piano with crisp hammer attack transients.
* **`DUAL LEAD`**: Expressive dual-carrier synthesizer lead for solo melodies.
* **`STACK BASS`**: Punchy, gritty FM bass with deep low-end definition.
* **`ORGAN 4`**: 4-harmonic additive tonewheel organ.
* **`CRYSTAL`**: Shimmering, ethereal bell chimes.
* **`METALLIC`**: Bright percussion and industrial metallic strikes.
* **`WARM SINE`**: Smooth, mellow foundation synth.
* **`VELVET PAD`**: Slow-attack evolving FM ambient pad.
