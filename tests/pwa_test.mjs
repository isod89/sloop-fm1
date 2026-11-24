// SPDX-License-Identifier: GPL-3.0-only
// SLOOP 2.6: the site without internet (web/sw.js, made by web/make_site.py), in a real Chromium (Playwright):
// opened once with the server up, the installer (and its firmware, checked against its SHA-256) and the editor
// load again with the server stopped; online again, a changed page is served as published, never the old copy;
// the manifest is one Chrome can install. Stable and beta on one origin keep independent offline copies.
// Run from the repo root after a site is made:
//   node tests/pwa_test.mjs SITE_DIR      (skipped, exit 0, without Playwright)
import { execFileSync, spawn } from "node:child_process";
import { createHash } from "node:crypto";
import { cpSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { createRequire } from "node:module";
import { tmpdir } from "node:os";
import { basename, join } from "node:path";

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
const origin = `http://127.0.0.1:${PORT}`;
const base = origin + "/webapp/";
let failed = 0;
const ok = (c, what) => { console.log(`pwa: ${what.padEnd(70)} ${c ? "ok" : "FAIL"}`); if (!c) failed++; };
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

let server = null;
async function serve(dir = site, probe = base) {
  server = spawn("python3", ["-m", "http.server", String(PORT), "--bind", "127.0.0.1", "--directory", dir],
                 { stdio: "ignore" });
  for (let i = 0; i < 50; i++) {
    try { if ((await fetch(probe + "sw.js")).ok) return; } catch { await sleep(100); }
  }
  throw new Error("server did not start");
}
async function stop() { if (server) { server.kill(); await new Promise((r) => server.on("exit", r)); server = null; } }

function makeSite(pkg, version, out, ...flags) {
  execFileSync("python3", ["web/make_site.py", pkg, version, out, ...flags], { stdio: "inherit" });
}

async function openApp(ctx, scope) {
  const page = await ctx.newPage();
  await page.goto(scope + "editor/");
  await page.waitForFunction((url) => navigator.serviceWorker.controller?.scriptURL === url, scope + "sw.js");
  return page;
}

const cacheKeys = (page) => page.evaluate(() => caches.keys());
const cachePrefix = (scope) => "sloop-scope-" + encodeURIComponent(scope) + ":";
const stamp = (worker) => worker.match(/const STAMP = "([^"]+)"/)[1];
const installerMeta = (html) => JSON.parse(html.match(/const meta = (.*);/)[1]);
const sha256 = (bytes) => createHash("sha256").update(bytes).digest("hex");

async function checkOffline(page, scope, name, cache, hash) {
  const editor = await page.goto(scope + "editor/").catch(() => null);
  ok(editor?.status() === 200 && editor.fromServiceWorker() && await page.title() === "SLOOP Editor",
     `${name}: offline editor (${editor?.status() ?? "network error"})`);
  const installer = await page.goto(scope + "installer/").catch(() => null);
  ok(installer?.status() === 200 && installer.fromServiceWorker(),
     `${name}: offline installer (${installer?.status() ?? "network error"})`);
  if (installer?.status() !== 200) return;
  await page.waitForFunction(() => document.getElementById("status").textContent.startsWith("SLOOP "), null,
                             { timeout: 5000 }).catch(() => {});
  ok((await page.textContent("#status")).startsWith("SLOOP "), `${name}: installer accepts the offline package`);
  const meta = installerMeta(await page.content());
  const url = new URL(meta.pkg, page.url()).href;
  const cachedHash = await page.evaluate(async ({ url, cacheName }) => {
    const response = await caches.match(url, { cacheName });
    if (!response) return null;
    const digest = await crypto.subtle.digest("SHA-256", await response.arrayBuffer());
    return [...new Uint8Array(digest)].map((b) => b.toString(16).padStart(2, "0")).join("");
  }, { url, cacheName: cache });
  ok(url === new URL("../firmware/" + basename(pkg), scope).href &&
     meta.sha256 === hash && cachedHash === meta.sha256,
     `${name}: its cache holds the package at the deployment path with the advertised hash`);
}

async function updateWorker(page) {
  return page.evaluate(async () => {
    const reg = await navigator.serviceWorker.getRegistration();
    const state = new Promise((resolve) => {
      reg.addEventListener("updatefound", () => {
        const worker = reg.installing;
        const finished = () => {
          if (worker.state === "activated" || worker.state === "redundant") resolve(worker.state);
        };
        worker.addEventListener("statechange", finished);
        finished();
      }, { once: true });
    });
    await reg.update();
    return Promise.race([state, new Promise((resolve) => setTimeout(() => resolve("timed out"), 10000))]);
  });
}

async function checkDeployments(browser, dir) {
  const stableDir = join(dir, "sloop-fm1");
  const betaDir = join(stableDir, "beta");
  const stable = origin + "/sloop-fm1/webapp/";
  const beta = origin + "/sloop-fm1/beta/webapp/";
  const mirror = origin + "/mirror/webapp/";
  const stableBytes = readFileSync(fw);
  // Same package name, distinct bytes, so an installer cannot accept the other deployment's response.
  const betaBytes = Buffer.from(stableBytes);
  betaBytes[betaBytes.length - 1] ^= 0x55;
  const betaPkg = join(dir, "beta.fwsc");
  writeFileSync(betaPkg, betaBytes);
  makeSite(fw, version, stableDir);
  cpSync(stableDir, join(dir, "mirror"), { recursive: true });
  makeSite(betaPkg, version, betaDir, "--beta");
  const stableSw = join(stableDir, "webapp", "sw.js");
  const betaSw = join(betaDir, "webapp", "sw.js");
  const stableWorker = readFileSync(stableSw, "utf8");
  const betaWorker = readFileSync(betaSw, "utf8");
  const stableCache = cachePrefix(stable) + stamp(stableWorker);
  const betaCache = cachePrefix(beta) + stamp(betaWorker);
  const mirrorCache = cachePrefix(mirror) + stamp(stableWorker);
  const stableHash = sha256(stableBytes);
  const betaHash = sha256(betaBytes);

  await serve(dir, stable);
  const ctx = await browser.newContext();
  try {
    const stablePage = await openApp(ctx, stable);
    ok(await stablePage.evaluate(async (cacheName) => !!await caches.match(location.href, { cacheName }), stableCache),
       "stable: editor precached before beta opens");
    await stablePage.evaluate(() => caches.open("unrelated").then(() => {}));
    const betaPage = await openApp(ctx, beta);
    const keys = await cacheKeys(betaPage);
    ok(keys.includes(stableCache) && keys.includes(betaCache), "beta activation preserves stable's cache");
    const mirrorPage = await openApp(ctx, mirror);
    const mirroredKeys = await cacheKeys(mirrorPage);
    ok(mirroredKeys.includes(stableCache) && mirroredKeys.includes(mirrorCache),
       "identical output under two roots gets two cache names");
    await stop();
    await checkOffline(stablePage, stable, "stable", stableCache, stableHash);
    await checkOffline(betaPage, beta, "beta", betaCache, betaHash);

    await serve(dir, stable);
    const nextBytes = Buffer.from(stableBytes);
    nextBytes[nextBytes.length - 1] ^= 0x33;
    const nextPkg = join(dir, "next.fwsc");
    writeFileSync(nextPkg, nextBytes);
    makeSite(nextPkg, version, stableDir);
    const nextCache = cachePrefix(stable) + stamp(readFileSync(stableSw, "utf8"));
    const font = join(stableDir, "webapp", "editor", "fukiai.ttf");
    const fontBytes = readFileSync(font);
    rmSync(font);
    ok(await updateWorker(stablePage) === "redundant", "stable: an update missing a required file fails to install");
    await stop();
    await checkOffline(stablePage, stable, "after failed update", stableCache, stableHash);

    writeFileSync(font, fontBytes);
    await serve(dir, stable);
    ok(await updateWorker(stablePage) === "activated", "stable: the complete update activates");
    const updated = await cacheKeys(stablePage);
    ok(updated.filter((k) => k.startsWith(cachePrefix(stable))).join() === nextCache,
       "stable activation replaces its previous cache");
    ok([betaCache, mirrorCache, "unrelated"].every((k) => updated.includes(k)),
       "stable activation preserves peer and unrelated caches");
    await stop();
    await checkOffline(stablePage, stable, "updated stable", nextCache, sha256(nextBytes));
    await checkOffline(betaPage, beta, "untouched beta", betaCache, betaHash);
  } finally {
    await ctx.close();
  }

  // Recreate the old worker's naming and cleanup while keeping the generated beta assets and stamp.
  makeSite(fw, version, stableDir);
  writeFileSync(betaSw, betaWorker
    .replace("const CACHE = CACHE_PREFIX + STAMP;", 'const CACHE = "sloop-site-" + STAMP;')
    .replace("k.startsWith(CACHE_PREFIX)", 'k.startsWith("sloop-site-")'));
  const legacyCache = "sloop-site-" + stamp(betaWorker);
  for (const first of [stable, beta]) {
    await serve(dir, stable);
    const mixed = await browser.newContext();
    try {
      const pages = new Map();
      for (const scope of [first, first === stable ? beta : stable]) {
        pages.set(scope, await openApp(mixed, scope));
      }
      const order = first === stable ? "new worker first" : "old worker first";
      const keys = await cacheKeys(pages.get(stable));
      ok(keys.includes(stableCache) && keys.includes(legacyCache), `${order}: both caches survive activation`);
      await stop();
      await checkOffline(pages.get(stable), stable, `${order}, stable`, stableCache, stableHash);
      await checkOffline(pages.get(beta), beta, `${order}, legacy beta`, legacyCache, betaHash);
    } finally {
      await mixed.close();
    }
  }
}

const ix = join(site, "webapp", "installer", "index.html");
const ix0 = readFileSync(ix, "utf8");
const { pkg, version } = installerMeta(ix0);
const fw = join(site, "webapp", "installer", pkg);

const launch = { headless: true };
if (process.env.PLAYWRIGHT_BROWSERS_PATH === undefined) launch.executablePath = "/opt/pw-browsers/chromium";
const browser = await chromium.launch(launch).catch(() => chromium.launch({ headless: true }));
const fixtures = mkdtempSync(join(tmpdir(), "sloop-pwa-"));
try {
  await checkDeployments(browser, fixtures);
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
  const swf = join(site, "webapp", "sw.js");
  const sw0 = readFileSync(swf, "utf8");
  const cacheName = cachePrefix(base) + stamp(sw0);
  const cached = await page.evaluate(async (name) => {
    if (!await caches.has(name)) return [];
    const c = await caches.open(name);
    return (await c.keys()).map((r) => new URL(r.url).pathname);
  }, cacheName);
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
  writeFileSync(swf, sw0.replace(stamp(sw0), stamp(sw0) + "-next"));
  ok(await updateWorker(page) === "activated", "a new site: its sw.js activates");
  const ownKeys = (await cacheKeys(page)).filter((k) => k.startsWith(cachePrefix(base)));
  ok(ownKeys.join() === cacheName + "-next", "a new site: its sw.js replaces its previous copy");
  writeFileSync(swf, sw0);
  await stop();

  /* (2.7) a new build keeps the firmware's name: the copy the browser kept of the build before (a server that lets it
     keep one, no service worker) is not offered as "damaged": the installer asks the server first */
  const fw0 = readFileSync(fw);
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
  writeFileSync(ix, ix0.replace(/"sha256": "[0-9a-f]+"/, `"sha256": "${sha256(fw1)}"`));
  await p2.reload();
  await p2.waitForTimeout(800);
  const second = await p2.textContent("#status");
  ok(first === `SLOOP ${version}` && second === first, "a new build of the same name: the new firmware, not the browser's old copy");
  writeFileSync(fw, fw0); writeFileSync(ix, ix0);
  await c2.close();
} finally {
  await stop();
  await browser.close();
  rmSync(fixtures, { recursive: true, force: true });
}
console.log("pwa:", failed ? `FAILED (${failed})` : "the site without internet: test passed");
process.exit(failed ? 1 : 0);
