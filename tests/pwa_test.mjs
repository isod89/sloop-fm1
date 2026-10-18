// SPDX-License-Identifier: GPL-3.0-only
// SLOOP 2.6: the site without internet (web/sw.js, made by web/make_site.py), in a real Chromium (Playwright):
// opened once with the server up, the installer (and its firmware, checked against its SHA-256) and the editor
// load again with the server stopped; online again, a changed page is served as published, never the old copy;
// the manifest is one Chrome can install. Run from the repo root after a site is made:
//   node tests/pwa_test.mjs SITE_DIR      (skipped, exit 0, without Playwright)
import { spawn } from "node:child_process";
import { readFileSync, writeFileSync } from "node:fs";
import { createRequire } from "node:module";
import { join } from "node:path";

let chromium;
try { ({ chromium } = await import("playwright")); }
catch {
  try {                                              // (a global install: NODE_PATH, as require finds it)
    const req = createRequire(join(process.cwd(), "x.js"));
    ({ chromium } = req(req.resolve("playwright", { paths: (process.env.NODE_PATH || "").split(":").filter(Boolean) })));
  } catch { console.log("pwa: Playwright not installed: skipped"); process.exit(0); }
}

const site = process.argv[2];
if (!site) { console.log("usage: node tests/pwa_test.mjs SITE_DIR"); process.exit(2); }
const PORT = 8790 + Math.floor(Math.random() * 100);
const base = `http://127.0.0.1:${PORT}/webapp/`;
let failed = 0;
const ok = (c, what) => { console.log(`pwa: ${what.padEnd(70)} ${c ? "ok" : "FAIL"}`); if (!c) failed++; };
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

let server = null;
async function serve() {
  server = spawn("python3", ["-m", "http.server", String(PORT), "--bind", "127.0.0.1", "--directory", site],
                 { stdio: "ignore" });
  for (let i = 0; i < 50; i++) {
    try { await fetch(base + "sw.js"); return; } catch { await sleep(100); }
  }
  throw new Error("server did not start");
}
async function stop() { if (server) { server.kill(); await new Promise((r) => server.on("exit", r)); server = null; } }

const launch = { headless: true };
if (process.env.PLAYWRIGHT_BROWSERS_PATH === undefined) launch.executablePath = "/opt/pw-browsers/chromium";
const browser = await chromium.launch(launch).catch(() => chromium.launch({ headless: true }));
try {
  await serve();
  const ctx = await browser.newContext();
  const page = await ctx.newPage();
  const perr = [];
  page.on("pageerror", (e) => perr.push(e.message));
  await page.goto(base + "editor/");
  await page.waitForTimeout(500);
  ok(!perr.length, "the editor loads without a script error" + (perr.length ? ": " + perr.join("; ") : ""));
  ok(await page.evaluate(() => { const b = document.getElementById("install"); return !!b && b.hidden; }),
     "the editor: its Install the app button, hidden until Chrome offers to install");
  await page.goto(base + "installer/");
  await page.waitForTimeout(300);
  ok(!perr.length, "the installer loads without a script error" + (perr.length ? ": " + perr.join("; ") : ""));
  const ready = await page.evaluate(async () => {
    const reg = await navigator.serviceWorker.ready;
    return !!reg.active;
  });
  ok(ready, "online: the installer registers sw.js, it is active");
  await page.reload();                                   // (now controlled)
  ok(await page.evaluate(() => !!navigator.serviceWorker.controller), "the installer page is controlled by it");
  const cached = await page.evaluate(async () => {
    const ks = await caches.keys();
    const c = await caches.open(ks.find((k) => k.startsWith("sloop-site-")));
    return (await c.keys()).map((r) => new URL(r.url).pathname);
  });
  ok(cached.includes("/webapp/editor/") && cached.includes("/webapp/installer/") &&
     cached.some((p) => /^\/firmware\/sloop-.*\.fwsc$/.test(p)) && cached.includes("/webapp/editor/fukiai.ttf"),
     "kept: the installer, the editor, its font, the firmware");

  const cdp = await ctx.newCDPSession(page);
  const errs = (await cdp.send("Page.getInstallabilityErrors")).installabilityErrors || [];
  ok(errs.length === 0, "Chrome can install it (no installability error)" + (errs.length ? ": " + errs.map((e) => e.errorId).join(", ") : ""));

  await stop();                                          // ---- no network at all
  await page.goto(base + "installer/");
  await page.waitForFunction(() => /SLOOP \d/.test(document.getElementById("status").textContent), null, { timeout: 8000 })
    .catch(() => {});
  const st = await page.textContent("#status");
  ok(/^SLOOP \d/.test(st), `offline: the installer loads its firmware from the copy (${st.trim().slice(0, 40)})`);
  await page.goto(base + "editor/");
  const ed = await page.evaluate(async () => {
    await document.fonts.ready;
    return { title: document.title, connect: !!document.getElementById("connect"), font: document.fonts.check('16px "Fukiai"', "") };
  });
  ok(ed.title === "SLOOP Editor" && ed.connect, "offline: the editor loads");
  ok(ed.font, "offline: with its icon font");
  await page.goto(base + "editor/index.html?x=1");
  ok((await page.title()) === "SLOOP Editor", "offline: index.html and a query find the same copy");
  const miss = await page.goto(base + "nothing.html").then(() => "page").catch(() => "error");
  ok(miss === "error", "offline: what is not kept is not invented");

  await serve();                                         // ---- online again, the site changed
  const f = join(site, "webapp", "editor", "index.html");
  const orig = readFileSync(f, "utf8");
  writeFileSync(f, orig.replace("<title>SLOOP Editor</title>", "<title>SLOOP Editor NEW</title>"));
  await page.goto(base + "editor/");
  ok((await page.title()) === "SLOOP Editor NEW", "online: the page as published now, not the old copy");
  await stop();
  await page.goto(base + "editor/");
  ok((await page.title()) === "SLOOP Editor NEW", "offline after that: the copy is the newest one seen");
  writeFileSync(f, orig);

  await serve();                                         // ---- a new site: a new sw.js takes over, the old copy goes
  const swf = join(site, "webapp", "sw.js");
  const sw0 = readFileSync(swf, "utf8");
  writeFileSync(swf, sw0.replace(/const STAMP = "([^"]*)"/, 'const STAMP = "$1-next"'));
  const keys = await page.evaluate(async () => {
    const reg = await navigator.serviceWorker.getRegistration();
    await reg.update();
    for (let i = 0; i < 80 && !(await caches.keys()).some((k) => k.endsWith("-next")); i++)
      await new Promise((r) => setTimeout(r, 100));
    for (let i = 0; i < 40 && (await caches.keys()).length > 1; i++) await new Promise((r) => setTimeout(r, 100));
    return caches.keys();
  });
  ok(keys.length === 1 && keys[0].endsWith("-next"), "a new site: its sw.js replaces the copy (" + keys.join(", ") + ")");
  writeFileSync(swf, sw0);
  await stop();

  /* (2.7) a new build keeps the firmware's name: the copy the browser kept of the build before (a server that lets it
     keep one, no service worker) is not offered as "damaged": the installer asks the server first */
  const fw = join(site, "firmware", "sloop-2.7.fwsc"), ix = join(site, "webapp", "installer", "index.html");
  const fw0 = readFileSync(fw), ix0 = readFileSync(ix, "utf8");
  server = spawn("python3", ["-c", `import http.server, os
class H(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        if self.path.endswith(".fwsc"): self.send_header("Cache-Control", "max-age=3600")
        super().end_headers()
os.chdir(${JSON.stringify(site)}); http.server.ThreadingHTTPServer(("127.0.0.1", ${PORT}), H).serve_forever()`], { stdio: "ignore" });
  for (let i = 0; i < 50; i++) { try { await fetch(base + "sw.js"); break; } catch { await sleep(100); } }
  const c2 = await browser.newContext({ serviceWorkers: "block" });
  const p2 = await c2.newPage();
  await p2.addInitScript(() => { if (!navigator.requestMIDIAccess) navigator.requestMIDIAccess = () => Promise.reject(new Error("none")); });
  await p2.goto(base + "installer/");
  await p2.waitForTimeout(800);
  const first = await p2.textContent("#status");
  const fw1 = Buffer.from(fw0); fw1[fw1.length - 1] ^= 0x55;
  writeFileSync(fw, fw1);
  const { createHash } = await import("node:crypto");
  writeFileSync(ix, ix0.replace(/"sha256": "[0-9a-f]+"/, `"sha256": "${createHash("sha256").update(fw1).digest("hex")}"`));
  await p2.reload();
  await p2.waitForTimeout(800);
  const second = await p2.textContent("#status");
  ok(/SLOOP 2\.7/.test(first) && /SLOOP 2\.7/.test(second), "a new build of the same name: the new firmware, not the browser's old copy");
  writeFileSync(fw, fw0); writeFileSync(ix, ix0);
  await c2.close();
} finally {
  await stop();
  await browser.close();
}
console.log("pwa:", failed ? `FAILED (${failed})` : "the site without internet: test passed");
process.exit(failed ? 1 : 0);
