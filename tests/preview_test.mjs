// SPDX-License-Identifier: GPL-3.0-only
// Real firmware audio: WASM vs the native C adapter, plus playback/edit/stop.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import vm from 'node:vm';
import { FirmwareAudio } from '../web/preview-worklet.js';

const html = readFileSync('web/editor.html', 'utf8');
const proto = html.slice(html.indexOf('/*PROTO-BEGIN*/'), html.indexOf('/*PROTO-END*/'));
const E = vm.runInNewContext(proto + ';({makeMockDevice, previewSnapshot, previewToggleStep, previewSetStepNotes, previewToggleDrum, CMD, req, frame, unframe, FM6, buildSlot})',
  { setTimeout, clearTimeout, setInterval, clearInterval, console, TextEncoder, TextDecoder, Blob, Response, DecompressionStream });
const module = await WebAssembly.compile(readFileSync('build/preview/sloop-preview.wasm'));
assert.equal(WebAssembly.Module.imports(module).length, 0, 'DSP must have no hardware or runtime imports');
function device(options = {}) { return E.makeMockDevice({ auto: false, ...options }); }
function render(dsp, frames = 8192) {
  const l = new Float32Array(frames), r = new Float32Array(frames);
  dsp.render(l, r);
  assert([...l, ...r].every((v) => Number.isFinite(v) && Math.abs(v) <= 1), 'finite, bounded audio');
  return { l, r, energy: l.reduce((s, x) => s + Math.abs(x), 0) + r.reduce((s, x) => s + Math.abs(x), 0) };
}
async function request(m, [cmd, a]) {
  const inp = [...m.access.inputs.values()][0], out = [...m.access.outputs.values()][0];
  return new Promise((resolve) => { inp.onmidimessage = ({data}) => resolve(E.unframe(data)); out.send(E.frame(cmd, a)); });
}
for (let engine = 0; engine < 12; engine++) {
  const m = device();
  await request(m, E.req.set(1, 20, engine));
  await request(m, E.req.preset(engine, 0));
  const state = E.previewSnapshot(m.state);
  state.tracks[0].p[17] = 0; // direct note, no arp in this comparison
  const dsp = new FirmwareAudio(module); dsp.sync(state);
  const t = state.tracks[0], frames = 8192;
  const cfg = Int32Array.from([engine, t.preset, 60, frames, ...t.p, ...state.g]);
  const raw = execFileSync('build/host/preview_native', { input: Buffer.concat([Buffer.from(cfg.buffer), Buffer.from(t.fm6)]) });
  const native = new Float32Array(raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength));
  dsp.w.preview_note(0, 60, 1);
  const a = render(dsp, frames/2);
  dsp.w.preview_note(0, 60, 0);
  const b = render(dsp, frames/2);
  const left = [...a.l, ...b.l], right = [...a.r, ...b.r];
  assert(a.energy + b.energy > 0.01, `engine ${engine} must sound`);
  let delta = 0;
  for (let i = 0; i < frames; i++) delta = Math.max(delta, Math.abs(left[i] - native[i*2]), Math.abs(right[i] - native[i*2+1]));

  assert(delta <= 1/32768, `engine ${engine}: native/WASM delta ${delta}`);
  console.log(`engine ${engine}: native/WASM match (${delta})`);
  m.stop();
}
{
  const m = device(), dsp = new FirmwareAudio(module);
  dsp.sync(E.previewSnapshot(m.state, true)); dsp.w.preview_play(1);
  const initial = render(dsp, 44160); assert(initial.energy > 1);
  // Mixer changes must affect audio while the sequence runs.
  m.state.tracks.forEach(t => t.p[40] = 1);
  dsp.sync(E.previewSnapshot(m.state)); render(dsp, 8192);
  const muted = render(dsp, 44160); assert(muted.energy < initial.energy * 0.1);
  m.state.tracks.forEach(t => t.p[40] = 0);
  dsp.sync(E.previewSnapshot(m.state)); assert(render(dsp, 44160).energy > muted.energy * 2);
  dsp.w.preview_panic(); render(dsp, 44160 * 4);
  assert(render(dsp, 44160).energy < 0.001, 'Stop must release all notes and tails');
  // Custom ADPCM samples use RAM flash, never the hardware XIP address.
  const sample = Int16Array.from({length: 4096}, (_, i) => Math.round(10000 * Math.sin(i / 16)));
  const {hdr, data} = E.buildSlot('LOCAL', [{s: sample, root: 60}]);
  const flash = m.state.smp[0].flash; flash.set(hdr); flash.set(data, 512);
  m.state.tracks[0].engine = 4; m.state.tracks[0].p[63] = 8; m.state.tracks[0].p[17] = 0;
  dsp.sync(E.previewSnapshot(m.state, true)); render(dsp, 8192);
  dsp.w.preview_note(0, 60, 1); assert(render(dsp).energy > 0.01, 'uploaded sample must be audible');
  m.stop();
  console.log('sequence, live mute/unmute, stop, and uploaded samples: passed');
}
{
  let changes = 0;
  const m = device({onChange: () => { changes++; dsp.sync(E.previewSnapshot(m.state)); }});
  const dsp = new FirmwareAudio(module); dsp.sync(E.previewSnapshot(m.state));
  dsp.w.preview_play(1); const before = render(dsp, 44160);
  for (let i = 0; i < 4; i++) await request(m, E.req.trackMix(i, 100, 1));
  render(dsp, 44160 * 3);
  assert.equal(changes, 4);
  assert(render(dsp, 44160).energy < before.energy * 0.01, 'editor protocol edits must reach the audible mixer');
  m.stop(); console.log('editor protocol -> audio thread live edits: passed');
}
{
  // The UI follows firmware step indices, including independent DIV/LEN and nudges.
  const m = device(), dsp = new FirmwareAudio(module);
  const state = E.previewSnapshot(m.state);
  state.g[0] = 120; state.g[1] = 0;
  const lengths = [16, 7, 32, 5], divisions = [2, 1, 3, 2], denominators = [4, 2, 8, 4];
  state.tracks.forEach((t, k) => { t.p[29] = lengths[k]; t.p[30] = divisions[k]; t.p[31] = 0; t.micro.fill(0); });
  dsp.sync(state);
  assert.deepEqual(dsp.transport().positions, [-1, -1, -1, -1]);
  assert.equal(dsp.w.preview_position(99), -1);
  dsp.w.preview_play(1);
  let reachedSecondPage = false;
  for (let blocks = 1; blocks <= 24; blocks++) {
    render(dsp, 4096);
    const lastBlock = blocks * 4096 - dsp.frames;
    const expected = lengths.map((len, k) => Math.floor(lastBlock * 120 * denominators[k] / (44100 * 60)) % len);
    assert.deepEqual(dsp.transport().positions, expected, 'playhead must use the clock producing the audio');
    reachedSecondPage ||= expected[2] >= 16;
  }
  assert(reachedSecondPage, 'long patterns must reach their second page');
  dsp.w.preview_panic(); render(dsp, dsp.frames);
  assert.deepEqual(dsp.transport().positions, [-1, -1, -1, -1], 'stopped transport has no active steps');
  dsp.w.preview_play(1); render(dsp, dsp.frames);
  assert.deepEqual(dsp.transport().positions, [0, 0, 0, 0], 'restart begins at the first step');
  dsp.w.preview_panic(); render(dsp, dsp.frames);
  const nudged = structuredClone(state);
  nudged.tracks[0].micro[0] = 16; dsp.sync(nudged);
  dsp.w.preview_play(1); render(dsp, 512);
  assert.equal(dsp.transport().positions[0], -1, 'a delayed first step has not fired yet');
  render(dsp, 1024);
  assert.equal(dsp.transport().positions[0], 0, 'the playhead moves when the nudged step fires');
  m.stop(); console.log('audio playhead: independent lengths/divisions, paging, stop/restart, and micro timing passed');
}
{
  const m = device({onChange: () => dsp.sync(E.previewSnapshot(m.state))});
  const dsp = new FirmwareAudio(module); dsp.sync(E.previewSnapshot(m.state));
  // A click on another track must not change selection or overwrite a chord elsewhere.
  const original = JSON.stringify(m.state.tracks[0].step);
  const step = m.state.tracks[1].step[0];
  const added = E.previewToggleStep(step, 67);
  assert.equal(step.n, 0, 'toggle does not mutate the source');
  await request(m, E.req.trackStep(1, 0, added));
  assert.equal(m.state.sel, 0);
  assert.equal(m.state.tracks[1].step[0].notes[0], 67);
  assert.equal(JSON.stringify(m.state.tracks[0].step), original);
  // Isolate the edited track: a new note must be audible, then removed through the same protocol.
  for (const k of [0, 2, 3]) await request(m, E.req.trackMix(k, 100, 1));
  dsp.w.preview_play(1); assert(render(dsp, 44160).energy > 0.01, 'inline note sounds in the loop');
  await request(m, E.req.trackStep(1, 0, E.previewToggleStep(m.state.tracks[1].step[0], 67)));
  dsp.w.preview_panic(); render(dsp, 44160 * 4); dsp.w.preview_play(1);
  assert(render(dsp, 44160).energy < 0.001, 'removed note leaves a silent loop');
  await request(m, E.req.trackStep(1, 0, E.previewToggleStep(m.state.tracks[1].step[0], 60)));
  assert.equal(m.state.tracks[1].step[0].notes[0], 67, 'reactivation restores G4 instead of the default C4');
  assert.equal(m.state.tracks[1].step[0].time, 0);
  dsp.w.preview_panic(); render(dsp, 44160 * 4); dsp.w.preview_play(1);
  assert(render(dsp, 44160).energy > 0.01, 'restored note is audible');
  const chord = { n: 3, notes: [45, 52, 57, 0], time: 0, flags: 3, vel: 84, lvl: 57, rat: 36 };
  assert.equal(E.previewToggleStep({...chord, time: 1}, 60).time, 2, 'a tie with stored pitches is disabled rather than retriggered');
  await request(m, E.req.trackStep(0, 0, chord));
  await request(m, E.req.trackStep(0, 0, E.previewToggleStep(m.state.tracks[0].step[0], 60)));
  assert.equal(m.state.tracks[0].step[0].time, 2);
  await request(m, E.req.trackStep(0, 0, E.previewToggleStep(m.state.tracks[0].step[0], 60)));
  assert.equal(JSON.stringify(m.state.tracks[0].step[0]), JSON.stringify(chord), 'toggle preserves chords, velocity, flags, levels and ratchets');
  const edited = E.previewSetStepNotes(m.state.tracks[0].step[0], [48, 55, 60]);
  await request(m, E.req.trackStep(0, 0, edited));
  assert.equal(m.state.tracks[0].step[0].notes.join(','), '48,55,60,0');
  assert.equal(m.state.tracks[0].step[0].vel, 84);
  assert.equal(m.state.tracks[0].step[0].lvl, 57);
  assert.equal(chord.notes[0], 45, 'editing does not mutate the original chord');
  // Toggling one lane preserves the other 15 lanes, including their levels and ratchets.
  const dr = {on: (1 << 2) | (1 << 4), lvl: new Array(16).fill(0), rat: new Array(16).fill(0)};
  dr.lvl[2] = 3; dr.rat[2] = 2; dr.lvl[4] = 1;
  await request(m, E.req.drumStep(0, dr));
  const before = JSON.stringify(m.state.tracks[3].dstep[0]);
  await request(m, E.req.drumStep(0, E.previewToggleDrum(m.state.tracks[3].dstep[0], 0)));
  const after = m.state.tracks[3].dstep[0];
  assert(after.on & 1); assert.equal(after.lvl[2], 3); assert.equal(after.rat[2], 2); assert.equal(after.lvl[4], 1);
  await request(m, E.req.drumStep(0, E.previewToggleDrum(after, 0)));
  assert.equal(JSON.stringify(m.state.tracks[3].dstep[0]), before, 'two drum clicks restore the same step');
  assert.equal(m.state.sel, 0);
  m.stop(); console.log('inline edits: add/remove audible notes, independent tracks, and preserved drum lanes passed');
}
console.log('preview audio tests passed');
