// SPDX-License-Identifier: GPL-3.0-only
// Run with Playwright and Chromium installed:
//   node tests/editor_reconnect_test.mjs [editor.html]
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
const { chromium } = require("playwright");
const html = readFileSync(process.argv[2] || new URL("../web/editor.html", import.meta.url), "utf8");
const url = "http://localhost/editor/";
const browser = await chromium.launch({ executablePath: process.env.CHROMIUM_EXECUTABLE });
try {
  const page = await browser.newPage({ locale: "en-US", serviceWorkers: "block" });
  const errors = [];
  page.on("pageerror", (err) => errors.push(err.message));
  await page.route("http://localhost/**", (route) => route.fulfill(
    route.request().isNavigationRequest() ? { contentType: "text/html", body: html } : { status: 404 }));
  await page.addInitScript(() => {
    const access = { sysexEnabled: true, inputs: new Map(), outputs: new Map(), onstatechange: null };
    let device, input, output, held, hold;
    const requests = [];
    Object.defineProperty(navigator, "requestMIDIAccess", { value: async () => access });
    window.midiTest = {
      requests,
      plug(legacy) {
        device = makeMockDevice({ auto: false, legacy });
        [input] = device.access.inputs.values();
        [output] = device.access.outputs.values();
        const send = output.send.bind(output);
        output.send = (data) => {
          const command = unframe(data)?.cmd;
          const name = Object.keys(CMD).find((key) => CMD[key] === command);
          requests.push(name);
          if (hold === name) { held = () => send(data); return; }
          send(data);
        };
        access.inputs.set(input.id, input);
        access.outputs.set(output.id, output);
        access.onstatechange?.({ port: output });
      },
      unplug() {
        input.state = output.state = "disconnected";
        access.inputs.clear();
        access.outputs.clear();
        held = hold = null;
        access.onstatechange({ port: output });
      },
      hold(command) { hold = command; },
      get pending() { return !!held; },
      resume() { const send = held; held = hold = null; send(); },
      level(value) { device.sim.level(value); },
      reload() { device.sim.reload(); },
    };
  });

  const command = (method, arg) => page.evaluate(([method, arg]) => midiTest[method](arg), [method, arg]);
  const count = () => page.evaluate(() => midiTest.requests.length);
  const after = (from) => page.evaluate((from) => midiTest.requests.slice(from), from);
  const wait = (fn, arg) => page.waitForFunction(fn, arg, { timeout: 9000 });
  const status = (text) => wait((text) => document.querySelector("#status").textContent === text, text);
  const ready = async () => {
    await status("Connected");
    await wait(() => !document.querySelector("#connect").disabled);
  };
  const poll = async (from) => {
    try {
      await wait((from) => {
        const names = midiTest.requests.slice(from);
        const dump = names.indexOf("DUMP");
        return dump >= 0 && names.indexOf("STEP_GET", dump + 1) >= 0;
      }, from);
    } catch (err) {
      throw new Error("Expected periodic DUMP and step reads after load. Saw: " + (await after(from)).join(", "), { cause: err });
    }
  };
  const reconnect = async (legacy) => {
    await command("unplug");
    await status("The FM-1 was unplugged");
    await command("plug", legacy);                  // makeMockDevice reuses the same port IDs
    await ready();
  };
  const hold = async (name) => command("hold", name);
  const pending = () => wait(() => midiTest.pending);
  const holdReloadSteps = async () => {
    const button = page.locator("#reloadsteps");
    await button.evaluate((b) => b.addEventListener("click", () => midiTest.hold("STEP_GET"), { once: true, capture: true }));
    await button.click();
    await pending();
  };

  await page.goto(url + "#sound");
  await command("plug", true);
  await page.locator("#connect").click();
  await ready();
  await poll(await count());
  await hold("DUMP");
  await pending();
  await reconnect(true);
  const reconnected = await count();
  const level = page.locator('#p-sound input[title="LVL"]').first();
  assert.notEqual(await level.inputValue(), "57");
  await command("level", 57);
  await poll(reconnected);
  await wait(() => document.querySelector('#p-sound input[title="LVL"]').value === "57");
  console.log("editor: DUMP and step reads resume after a dropped poll; the device level reaches the UI");

  await page.locator('button[data-tab="sequencer"]').click();
  await holdReloadSteps();
  await wait(() => document.querySelector("#status").textContent.startsWith("Reading steps"));
  await reconnect(true);
  await poll(await count());
  console.log("editor: polling resumes after disconnect interrupts Reload Steps");

  await reconnect(false);
  await wait(() => document.querySelector("#live").textContent.includes("Live"));
  await holdReloadSteps();
  const beforeReload = await count();
  await command("reload");
  await page.waitForTimeout(450);                   // at least one polling interval while held
  assert.ok(!(await after(beforeReload)).includes("DUMP"), "RELOAD must wait for Reload Steps");
  await command("resume");
  await status("The sound changed on the device: read again");
  const reloadRequests = await after(beforeReload);
  const dump = reloadRequests.indexOf("DUMP");
  const fill = reloadRequests.indexOf("FILL_GET");
  assert.ok(fill >= 0 && dump > fill, "deferred RELOAD must read DUMP after the manual step and fill reads");
  const afterReload = await count();
  await wait((from) => midiTest.requests.slice(from).includes("PING"), afterReload);
  console.log("editor: WATCH defers RELOAD while busy, reloads after release, and keeps sending PING");

  await page.locator("#connect").click();
  await status("Disconnected");
  const disconnected = await count();
  await command("unplug");
  await command("plug", false);
  await page.waitForTimeout(1100);                   // beyond the 800 ms auto-reconnect delay
  assert.equal(await count(), disconnected);
  assert.equal(await page.locator("#status").textContent(), "Disconnected");
  assert.deepEqual(errors, []);
  console.log("editor: manual Disconnect clears auto-reconnect intent");
} finally {
  await browser.close();
}
