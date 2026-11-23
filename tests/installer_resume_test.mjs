// SPDX-License-Identifier: GPL-3.0-only
// Run with Playwright and Chromium installed:
//   node tests/installer_resume_test.mjs RELEASE_SITE [DEV_SITE [EXPLICIT_RELEASE_SITE]]
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { createRequire } from "node:module";
import { createServer } from "node:net";

const require = createRequire(import.meta.url);
const { chromium } = require("playwright");

async function serve(directory) {
  const socket = createServer();
  await new Promise((resolve) => socket.listen(0, "127.0.0.1", resolve));
  const { port } = socket.address();
  await new Promise((resolve) => socket.close(resolve));
  const server = spawn("python3", ["-m", "http.server", String(port), "--bind", "127.0.0.1", "--directory", directory],
    { stdio: "ignore" });
  const url = `http://127.0.0.1:${port}/webapp/installer/`;
  for (let i = 0; i < 50; i++) {
    try { if ((await fetch(url)).ok) return { server, url }; } catch {}
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
  server.kill();
  throw new Error("Installer site did not start");
}

function mockMidi({ scenario, release }) {
  const midi = new EventTarget();
  midi.inputs = new Map();
  midi.outputs = new Map();
  const stats = window.midiStats = { upgrades: [], info: [], statuses: [], earlyProgress: false };
  window.addEventListener("DOMContentLoaded", () => {
    new MutationObserver(() => {
      const status = document.querySelector("#status").textContent;
      stats.statuses.push(status);
      if (status !== "Done" && !status.startsWith("App boot confirmed") && document.querySelector("#bar").value === 100)
        stats.earlyProgress = true;
    }).observe(document.querySelector("#status"), { childList: true });
  });

  let serial = 0;
  function sendPacket(data) {
    const bytes = [240];
    let bits = 0, pending = 0;
    for (const b of data) {
      pending |= b << bits;
      bits += 8;
      while (bits >= 7) {
        bytes.push(pending & 127);
        pending >>>= 7;
        bits -= 7;
      }
    }
    if (bits) bytes.push(pending & 127);
    return [...bytes, 247];
  }

  function boot(name, identity, unit) {
    const id = ++serial;
    const input = { id: "i" + id, name, state: "connected", onmidimessage: null };
    const output = { id: "o" + id, name, state: "connected" };
    for (const port of [input, output]) {
      port.connection = "closed";
      port.open = async () => { port.connection = "open"; };
      port.close = async () => { port.connection = "closed"; };
    }
    const emit = () => midi.dispatchEvent(new Event("statechange"));
    const tx = (bytes) => setTimeout(() => input.onmidimessage?.({ data: Uint8Array.from(bytes) }), 5);
    function request(addr, len) {
      const bytes = [0, 89, 48, 0, 0, 0, 0, addr & 255, addr >>> 8 & 255, addr >>> 16 & 255, addr >>> 24, len, 0, 0];
      bytes.push(~bytes.slice(6).reduce((a, b) => a + b, 0) & 255);
      tx(sendPacket(bytes));
    }
    function disconnect() {
      input.state = output.state = "disconnected";
      midi.inputs.delete(input.id);
      midi.outputs.delete(output.id);
      emit();
    }

    const loader = identity.startsWith("ota-");
    let replies = 0, identityQueries = 0, infoQueries = 0;
    output.send = (packet) => {
      if (packet[1] === 0 && packet.length === 10) {
        if (!loader && scenario === "late" && ++identityQueries < 2) return;
        const text = [...new TextEncoder().encode(identity)];
        tx(sendPacket([0, 89, 17, 0, 0, 0, ...text, ...new Array(28 - text.length).fill(0)]));
      } else if (packet[1] === 0x22 && packet.length === 6) {
        stats.upgrades.push(unit);
        request(0, 8);
      } else if (packet[1] === 0x7d) {
        stats.info.push(unit);
        if (scenario === "missing" || (scenario === "late" && ++infoQueries < 2)) return;
        const version = scenario === "old" ? "2.8.4" : release;
        const text = [...new TextEncoder().encode("FELUCCA SLOOP " + version)];
        const info = [240, 0x7d, 0x46, 0x4c, 1, ...text, 0, 1, 71, 45, 16, 63, 65, 0, 4, 12, 247];
        if (scenario === "malformed") info[4] = 2;
        tx(info);
      } else if (++replies === 1) {
        request(loader ? 0xf0000000 : 0xe0000000, 8);
      } else if (scenario === "ack") {
        throw new Error("Simulated disconnected output");
      } else {
        setTimeout(() => {
          disconnect();
          if (!loader) {
            boot("USB MIDI Device", "ota-FM-1_900", unit);
            return;
          }
          if (scenario === "other") boot("FM-1 B", "FM-1_900", "B");
          const rescue = scenario === "rescue" || scenario === "other";
          setTimeout(() => boot("Felucca", rescue ? "FM-1_000" : "FM-1_900", unit), scenario === "other" ? 800 : 50);
        }, scenario === "other" ? 2900 : 100);
      }
    };
    midi.inputs.set(input.id, input);
    midi.outputs.set(output.id, output);
    emit();
  }

  if (scenario === "fresh") boot("FM-1", "FM-1_015", "A");
  else boot("USB MIDI Device", scenario === "foreign" ? "ota-FM-1_015" : "ota-FM-1_900", "A");
  Object.defineProperty(navigator, "requestMIDIAccess", { value: async () => midi });
}

const [releaseSite, devSite, explicitSite] = process.argv.slice(2);
assert.ok(releaseSite, "Pass a generated release site directory");
const release = process.env.SLOOP_TEST_RELEASE || "2.8.5";
const servers = [];
let browser;
try {
  for (const path of [releaseSite, devSite, explicitSite].filter(Boolean)) servers.push(await serve(path));
  browser = await chromium.launch({ executablePath: process.env.CHROMIUM_EXECUTABLE });
  const cases = [
    ["rescue", /reports another version: FM-1_000/],
    ["late", /^Done$/],
    ["fresh", /^Done$/],
    ["old", /reports another version: SLOOP 2.8.4/],
    ["missing", /did not answer the editor INFO query/],
    ["malformed", /did not answer the editor INFO query/],
    ["other", /identify a single FM-1/],
    ["foreign", /update mode of another firmware/],
    ["ack", /USB connection was lost/],
  ].map(([scenario, expected]) => ({ scenario, expected, url: servers[0].url, release }));
  if (devSite) cases.push({ scenario: "dev", expected: /^App boot confirmed: SLOOP 2.8.5$/, url: servers[1].url, release: "2.8.5" });
  if (explicitSite) cases.push({ scenario: "explicit", expected: /^Done$/, url: servers[2].url, release });
  const filter = process.env.SLOOP_TEST_CASE;
  const outcomes = await Promise.allSettled(cases.filter(({ scenario }) => !filter || filter === scenario)
    .map(async ({ scenario, expected, url, release }) => {
      const page = await browser.newPage({ locale: "en-US", serviceWorkers: "block" });
      try {
        const errors = [];
        page.on("pageerror", (error) => errors.push(error.message));
        await page.addInitScript(mockMidi, { scenario, release });
        await page.goto(url);
        await page.locator("#go:enabled").click();
        await page.waitForFunction(() => !document.querySelector("#go").disabled, null, { timeout: 20000 });
        const status = await page.locator("#status").textContent();
        console.log(`${scenario}: ${status}`);
        assert.match(status, expected);
        assert.deepEqual(errors, []);
        const stats = await page.evaluate(() => window.midiStats);
        assert.deepEqual(stats.upgrades, scenario === "foreign" ? [] : scenario === "fresh" ? ["A", "A"] : ["A"]);
        assert.ok(stats.info.every((unit) => unit === "A"));
        const success = ["late", "fresh", "dev", "explicit"].includes(scenario);
        assert.equal(await page.locator("#bar").evaluate((bar) => bar.value === 100), success);
        assert.equal(stats.statuses.filter((status) => status === "Done" || status.startsWith("App boot confirmed")).length, Number(success));
        assert.equal(stats.earlyProgress, false);
        if (success) assert.ok(stats.info.length >= (scenario === "late" ? 2 : 1));
        if (["rescue", "late", "fresh", "old", "missing", "malformed", "dev", "explicit"].includes(scenario))
          assert.ok(stats.statuses.includes("Restarting"));
      } finally { await page.close(); }
    }));
  const failures = outcomes.filter((result) => result.status === "rejected");
  if (failures.length) throw new AggregateError(failures.map((result) => result.reason), "Installer resume scenarios failed");
} finally {
  await browser?.close();
  for (const { server } of servers) server.kill();
}
