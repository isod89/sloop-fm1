// SPDX-License-Identifier: GPL-3.0-only
// SLOOP live offline, always the latest online. Every file is asked of the network first (revalidated, past the HTTP
// cache) and kept; offline, or when the network takes longer than 3 s, the kept copy answers. A new version of this
// file (CACHE changes with every release) takes over at once; the page then offers a reload (app.js).
const CACHE = "sloop-live-6";
const FILES = ["./", "index.html", "app.js", "proto.js", "transports.js", "device.js", "widgets.js", "parts.js", "live.js",
  "arrange.js", "sound.js", "mix.js", "next.js", "help.js", "mock.js", "mockdata.js", "terminus.ttf",
  "manifest.webmanifest", "icon.svg", "icon-192.png", "icon-512.png"];
const WAIT_MS = 3000;

self.addEventListener("install", (e) => e.waitUntil(
  caches.open(CACHE).then((c) => c.addAll(FILES.map((f) => new Request(f, { cache: "reload" })))).then(() => self.skipWaiting())));
self.addEventListener("activate", (e) => e.waitUntil(
  caches.keys().then((ks) => Promise.all(ks.filter((k) => k !== CACHE).map((k) => caches.delete(k)))).then(() => self.clients.claim())));

self.addEventListener("fetch", (e) => {
  const req = e.request;
  if (req.method !== "GET" || new URL(req.url).origin !== location.origin) return;
  e.respondWith((async () => {
    const cache = await caches.open(CACHE);
    const kept = () => cache.match(req, { ignoreSearch: true }).then((r) => r || (req.mode === "navigate" ? cache.match("index.html") : undefined));
    const ctl = new AbortController();
    const t = setTimeout(() => ctl.abort(), WAIT_MS);
    try {
      const r = await fetch(req, { cache: "no-cache", signal: ctl.signal });
      clearTimeout(t);
      if (r.ok) cache.put(req, r.clone());
      return r;
    } catch (_) {
      clearTimeout(t);
      return (await kept()) || Response.error();
    }
  })());
});
