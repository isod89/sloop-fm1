// SPDX-License-Identifier: GPL-3.0-only
// SLOOP 2.6: the site without internet (webapp/sw.js, made by web/make_site.py). Opened once online, the
// installer, the editor and the firmware they install are kept here; offline they come from this copy, still on
// https, so Web MIDI asks as usual (a downloaded file on Android opens as content://, where Chrome refuses MIDI).
// Online the network always wins (the page as published, never an old one); the copy is only the fallback.
// A new site (any file changed) is a new sw.js: the new copy replaces the old one.
const STAMP = "/*STAMP*/";                          // version + a hash of the files
const CACHE = "sloop-site-" + STAMP;
const FILES = /*FILES*/[];                          // relative to this file
const KEEP = new Set(FILES.map((f) => new URL(f, self.location).href));

const key = (url) => {                              // the copy's name of a request: no query, index.html = the folder
  const u = new URL(url);
  u.search = "";
  u.hash = "";
  if (u.pathname.endsWith("/index.html")) u.pathname = u.pathname.slice(0, -10);
  return u.href;
};

self.addEventListener("install", (e) => {
  e.waitUntil((async () => {
    const c = await caches.open(CACHE);
    await c.addAll(FILES.map((f) => new Request(f, { cache: "reload" })));   // all or nothing: never half a site
    await self.skipWaiting();
  })());
});

self.addEventListener("activate", (e) => {
  e.waitUntil((async () => {
    for (const k of await caches.keys())
      if (k.startsWith("sloop-site-") && k !== CACHE) await caches.delete(k);
    await self.clients.claim();
  })());
});

self.addEventListener("fetch", (e) => {
  const req = e.request;
  if (req.method !== "GET") return;
  const k = key(req.url);
  if (!KEEP.has(k)) return;                         // not ours: the browser as usual
  e.respondWith((async () => {
    try {
      const r = await fetch(req.url, { cache: "no-cache" });   // online: what is published now (by its URL: a
                                                    // page's own request, mode navigate, takes no options)
      if (r.ok) {
        const copy = r.clone();
        e.waitUntil(caches.open(CACHE).then((c) => c.put(k, copy)).catch(() => {}));
      }
      return r;
    } catch (err) {                                 // offline: the copy
      const hit = await caches.match(k, { cacheName: CACHE });
      if (hit) return hit;
      throw err;
    }
  })());
});
