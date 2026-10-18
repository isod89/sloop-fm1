// SPDX-License-Identifier: GPL-3.0-only
// Browser-only firmware audio. Explicit start is required by autoplay policies.
export class LocalPreview {
  constructor(getState, onError, onLevel = () => {}, onTransport = () => {}) {
    this.getState = getState;
    this.onError = onError;
    this.onLevel = onLevel;
    this.onTransport = onTransport;
    this.context = null;
    this.node = null;
    this.loading = null;
    this.volume = 0.5;
  }
  async start() {
    if (!this.context) {
      const Audio = window.AudioContext || window.webkitAudioContext;
      if (!Audio || !window.AudioWorkletNode) throw new Error('AudioWorklet is not supported in this browser.');
      this.context = new Audio({ sampleRate: 44100, latencyHint: 'interactive' });
      if (this.context.sampleRate !== 44100) {
        await this.context.close(); this.context = null;
        throw new Error('SLOOP preview requires audio at 44.1 kHz.');
      }
    }
    // Resume immediately inside the click/key gesture, before the network awaits.
    await this.context.resume();
    if (!this.node && !this.loading) {
      this.loading = this.load().finally(() => { this.loading = null; });
    }
    if (this.loading) await this.loading;
    this.sync(true);
  }
  async load() {
    try {
      const response = await fetch(new URL('./sloop-preview.wasm', import.meta.url));
      if (!response.ok) throw new Error('Build the audio preview first: python3 tools/build_preview.py');
      const module = await WebAssembly.compile(await response.arrayBuffer());
      await this.context.audioWorklet.addModule(new URL('./preview-worklet.js', import.meta.url));
      this.node = new AudioWorkletNode(this.context, 'sloop-preview', {
        numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2], processorOptions: { module },
      });
      this.node.port.onmessage = ({ data }) => {
        if (data.error) this.onError(new Error(data.error));
        else {
          if (data.peak != null) this.onLevel(data.peak);
          if (data.transport) this.onTransport(data.transport);
        }
      };
      this.node.onprocessorerror = () => this.onError(new Error('The audio engine stopped. Disable audio and try again.'));
      this.gain = this.context.createGain();
      this.gain.gain.value = this.volume;
      this.node.connect(this.gain).connect(this.context.destination);
    } catch (error) {
      await this.context.close(); this.context = null; this.node = null;
      throw error;
    }
  }
  sync(includeSamples = false) {
    if (this.node) this.node.port.postMessage({ type: 'state', state: this.getState(includeSamples) });
  }
  play(on) { this.node?.port.postMessage({ type: 'play', on }); }
  note(track, note, on) { this.node?.port.postMessage({ type: 'note', track, note, on }); }
  hit(kit, lane, velocity) { this.node?.port.postMessage({ type: 'hit', kit, lane, velocity }); }
  panic() { this.node?.port.postMessage({ type: 'panic' }); }
  setVolume(value) {
    this.volume = value;
    if (this.gain) this.gain.gain.setTargetAtTime(value, this.context.currentTime, 0.01);
  }
  async close() {
    this.panic();
    if (this.loading) await this.loading.catch(() => {});
    this.node?.disconnect();
    if (this.context) await this.context.close();
    this.node = this.context = null;
  }
}
