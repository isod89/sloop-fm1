// SPDX-License-Identifier: GPL-3.0-only
// Shared audio fakes for the host and Chromium LIVE recorder tests.
export function recorderAudio(fail, cleanupThrows = false) {
  const tracks = [], contexts = [], nodes = [], urls = new Set(), captures = [];
  const devices = [
    { kind: "audioinput", deviceId: "default", label: "Default mic" },
    { kind: "audioinput", deviceId: "fm1", label: "SLOOP FM-1" },
  ];
  let load;
  const mediaDevices = {
    async getUserMedia(constraints) {
      captures.push(constraints.audio);
      if (constraints.audio === true && fail === "permission") throw new Error(fail);
      if (constraints.audio !== true && fail === "capture") throw new Error(fail);
      const track = {
        getSettings: () => ({ deviceId: "default" }),
        stops: 0,
        stop() { this.stops++; },
      };
      tracks.push(track);
      return { getTracks: () => [track], getAudioTracks: () => [track] };
    },
    async enumerateDevices() {
      if (fail === "enumerate") throw new Error(fail);
      return devices;
    },
  };

  function audioNode(kind) {
    return {
      connect(next) {
        this.next = next;
        if (kind === "mute" && fail === "connect") throw new Error(fail);
      },
      disconnect() {
        this.next = null;
        if (kind === "source" && cleanupThrows) throw new Error("disconnect");
      },
    };
  }
  class AudioWorkletNode {
    constructor() {
      Object.assign(this, audioNode("worklet"));
      this.port = { onmessage: null };
      nodes.push(this);
    }
  }
  class AudioContext {
    constructor() {
      contexts.push(this);
      this.destination = {};
      this.sampleRate = 48000;
      this.closed = false;
      this.audioWorklet = { addModule: async () => {
        if (fail === "defer") await new Promise((_, reject) => { load = reject; });
        if (fail === "worklet") throw new Error(fail);
      } };
    }
    createMediaStreamSource() { return this.source = audioNode("source"); }
    createGain() { return this.mute = { ...audioNode("mute"), gain: { value: 1 } }; }
    async close() {
      this.closed = true;
      if (cleanupThrows) throw new Error("close");
    }
  }
  return {
    mediaDevices, AudioContext, AudioWorkletNode, captures, devices, contexts,
    URL: {
      createObjectURL() { const url = `blob:worklet-${contexts.length}`; urls.add(url); return url; },
      revokeObjectURL(url) { urls.delete(url); },
    },
    failLoad() { fail = null; load(new Error("worklet")); },
    get loading() { return !!load; },
    samples(data) { nodes.at(-1).port.onmessage({ data: Float32Array.from(data) }); },
    counts() {
      return [
        tracks.filter((track) => !track.stops).length,
        contexts.filter((ctx) => !ctx.closed).length,
        urls.size,
        nodes.filter((node) => node.port.onmessage).length,
        [...nodes, ...contexts.flatMap((ctx) => [ctx.source, ctx.mute])].filter((node) => node?.next).length,
      ];
    },
  };
}
