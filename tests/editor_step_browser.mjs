// SPDX-License-Identifier: GPL-3.0-only
// Real browser step edits with a mock FM-1 on the Web MIDI boundary.
//   node tests/editor_step_browser.mjs [editor.html]
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { createServer } from "node:http";
import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
const { chromium } = require("playwright");
const html = readFileSync(process.argv[2] || new URL("../web/editor.html", import.meta.url), "utf8");
const proto = html.slice(html.indexOf("/*PROTO-BEGIN*/"), html.indexOf("/*PROTO-END*/"));
const server = createServer((_req, res) => res.writeHead(200, { "Content-Type": "text/html; charset=utf-8" }).end(html));
await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));

let browser;
try {
  const executablePath = process.env.CHROME_BIN;
  browser = await chromium.launch({ executablePath });
  for (const oldStepMetadata of [false, true]) {
    const page = await browser.newPage();
    const errors = [];
    page.on("pageerror", (error) => errors.push(error.message));
    await page.addInitScript(({ proto, oldStepMetadata }) => {
      const { makeMockDevice, unframe, CMD, parseNote } = new Function(proto + ";return { makeMockDevice, unframe, CMD, parseNote };")();
      const mock = makeMockDevice({ auto: false, oldStepMetadata });
      const input = mock.access.inputs.values().next().value;
      const output = mock.access.outputs.values().next().value;
      Object.assign(mock.state.tracks[0].step[0], {
        n: 2, notes: [60, 48, 0, 0], time: 0, flags: oldStepMetadata ? 1 : 0x91, vel: 96, lvl: 129, rat: 130,
      });
      window.stepMidi = { writes: [], reads: [], parseNote };
      const send = output.send.bind(output);
      output.send = (bytes) => {
        const f = unframe(bytes);
        if (f?.cmd === CMD.STEP_SET && f.a[0] === 0) window.stepMidi.writes.push(f.a);
        send(bytes);
      };
      let listener;
      Object.defineProperty(input, "onmidimessage", {
        set(handler) { listener = handler; },
        get() {
          return listener && ((event) => {
            const f = unframe(event.data);
            if (f?.cmd === CMD.STEP_GET && f.a[0] === 0) window.stepMidi.reads.push(f.a);
            listener(event);
          });
        },
      });
      Object.defineProperty(navigator, "requestMIDIAccess", { value: async () => mock.access });
    }, { proto, oldStepMetadata });
    await page.goto(`http://127.0.0.1:${server.address().port}`);
    await page.locator("#connect").click();
    await page.locator('#tabs [data-tab="sequencer"]').click();
    await page.locator("#steplist summary").click();
    const velocity = page.locator('#steptables tr[data-i="0"] input[type=number]');
    await velocity.fill("77");
    await velocity.press("Tab");
    await page.waitForFunction(() => window.stepMidi.writes.length === 1);
    const expected = [0, 2, 60, 48, 0, 0, 0, oldStepMetadata ? 1 : 17, 77, 1, 3, 2];
    if (!oldStepMetadata) expected.push(1);
    assert.deepEqual(await page.evaluate(() => window.stepMidi.writes[0]), expected, "step table velocity write");
    await page.locator("#reloadsteps").click();
    await page.waitForFunction(() => window.stepMidi.reads.some((args) => args[8] === 77));
    assert.deepEqual(await page.evaluate(() => window.stepMidi.reads.find((args) => args[8] === 77)), expected, "device reread");

    if (!oldStepMetadata) {
      await page.locator('#viewseg [data-v="roll"]').click();
      await page.locator("#rollcv").click({ position: await page.evaluate(() => {
        const top = window.stepMidi.parseNote(document.querySelector("#rollrange").textContent.split(" – ")[1]);
        return { x: 43, y: (top - 60) * 16 + 4 };
      }) });
      await page.waitForFunction(() => window.stepMidi.writes.length === 2);
      assert.deepEqual(await page.evaluate(() => window.stepMidi.writes[1]),
        [0, 1, 48, 0, 0, 0, 0, 9, 77, 1, 3, 2, 0], "piano roll moves the continuation bit to the remaining pitch");
    }
    assert.deepEqual(errors, []);
    console.log(oldStepMetadata ? "browser: legacy velocity edit and reread passed" : "browser: velocity edit, reread and piano roll removal passed");
    await page.close();
  }
} finally {
  await browser?.close();
  server.close();
}
