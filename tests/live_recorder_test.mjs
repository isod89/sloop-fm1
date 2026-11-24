// SPDX-License-Identifier: GPL-3.0-only
// Run with Playwright and Chromium installed:
//   node tests/live_recorder_test.mjs [editor.html]
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { createRequire } from "node:module";
import { recorderAudio } from "./live_recorder_audio.mjs";

const { chromium } = createRequire(import.meta.url)("playwright");
const html = readFileSync(process.argv[2] || new URL("../web/editor.html", import.meta.url), "utf8");
const browser = await chromium.launch({ executablePath: process.env.CHROMIUM_EXECUTABLE });
try {
  const page = await browser.newPage({ locale: "en-US", serviceWorkers: "block", acceptDownloads: true });
  const errors = [], downloads = [], empty = [0, 0, 0, 0, 0];
  page.on("pageerror", (err) => errors.push(err.message));
  page.on("download", (file) => downloads.push(file));
  await page.route("http://localhost/**", (route) => route.fulfill(
    route.request().isNavigationRequest() ? { contentType: "text/html", body: html } : { status: 404 }));
  await page.addInitScript({ content: `
    const audio = window.recorderTestAudio = (${recorderAudio.toString()})("defer");
    Object.defineProperty(navigator, "mediaDevices", { value: audio.mediaDevices });
    window.AudioContext = audio.AudioContext;
    window.AudioWorkletNode = audio.AudioWorkletNode;
    const createURL = URL.createObjectURL.bind(URL), revokeURL = URL.revokeObjectURL.bind(URL);
    URL.createObjectURL = (blob) => blob.type === "text/javascript" ? audio.URL.createObjectURL(blob) : createURL(blob);
    URL.revokeObjectURL = (url) => url.startsWith("blob:worklet-") ? audio.URL.revokeObjectURL(url) : revokeURL(url);
    localStorage.setItem("sloop-live-view", "all");
    localStorage.setItem("sloop-live-guide", "1");
  ` });

  await page.goto("http://localhost/editor/?mock=1");
  await page.locator("#connect").click();
  await page.locator('button[data-tab="live"]').click();
  const button = page.locator(".lvrecset");
  await button.dblclick();
  await page.waitForFunction(() => recorderTestAudio.loading);
  assert.equal(await page.evaluate(() => recorderTestAudio.captures.length), 2, "one permission request and one recording capture");
  assert.deepEqual(await page.evaluate(() => recorderTestAudio.counts()), [1, 1, 1, 0, 0]);
  assert.equal(await button.isDisabled(), true);
  assert.equal(await button.getAttribute("aria-pressed"), "false");
  await page.evaluate(() => recorderTestAudio.failLoad());
  await page.waitForFunction(() => /Recording is not possible here.*worklet/.test(document.querySelector("#status").textContent));
  assert.deepEqual(await page.evaluate(() => recorderTestAudio.counts()), empty);
  assert.equal(await button.isEnabled(), true);
  assert.equal(await button.getAttribute("aria-pressed"), "false");
  assert.equal(downloads.length, 0);

  await button.click();
  await page.waitForFunction(() => document.querySelector(".lvrecset").getAttribute("aria-pressed") === "true");
  assert.deepEqual(await page.evaluate(() => recorderTestAudio.captures[3].deviceId), { exact: "fm1" });
  assert.deepEqual(await page.evaluate(() => recorderTestAudio.counts()), [1, 1, 0, 1, 3]);
  assert.match(await button.textContent(), /stop rec.*0:\d\d/i);
  const samples = [0, 1, -1, 0.5];                     /* same fixture as the LVX.pcm16/LVX.wav host check */
  const expected = await page.evaluate((samples) => {
    recorderTestAudio.samples(samples);
    return Array.from(LVX.wav([LVX.pcm16(Float32Array.from(samples))], 48000, 2));
  }, samples);
  const stamp = () => page.evaluate(() => {
    const d = new Date();
    return `${d.toISOString().slice(0, 10)}-${String(d.getHours()).padStart(2, "0")}${String(d.getMinutes()).padStart(2, "0")}`;
  });
  const before = await stamp();
  const [file] = await Promise.all([page.waitForEvent("download"), button.click()]);
  const names = [before, await stamp()].map((value) => `SLOOP-live-${value}.wav`);
  assert.ok(names.includes(file.suggestedFilename()), file.suggestedFilename());
  const wav = readFileSync(await file.path());
  assert.deepEqual([...wav], expected);
  assert.equal(wav.toString("ascii", 0, 4), "RIFF");
  assert.equal(wav.toString("ascii", 8, 12), "WAVE");
  assert.equal(wav.readUInt16LE(20), 1);
  assert.equal(wav.readUInt16LE(22), 2);
  assert.equal(wav.readUInt32LE(24), 48000);
  assert.equal(wav.readUInt16LE(34), 16);
  assert.equal(wav.readUInt32LE(40), 8);
  assert.deepEqual([44, 46, 48, 50].map((offset) => wav.readInt16LE(offset)), [0, 32767, -32768, 16383]);
  assert.equal(wav.length, 52);
  assert.equal(await button.getAttribute("aria-pressed"), "false");
  assert.equal(await button.isEnabled(), true);
  assert.deepEqual(await page.evaluate(() => recorderTestAudio.counts()), empty);
  assert.equal(downloads.length, 1);
  assert.deepEqual(errors, []);
  console.log(`LIVE recorder: retry downloaded one 52-byte stereo PCM WAV (${file.suggestedFilename()}); all capture resources released`);
} finally {
  await browser.close();
}
