const CACHE = "missionlink-web-v7";
const ASSETS = ["./", "./index.html", "./outages.html", "./outages.css", "./outages.js", "./styles.css", "./app.js", "./testbench.html", "./testbench.css", "./testbench.js", "./flasher.html", "./flasher.css", "./flasher.js", "./flasher-validate.js", "./manifest.webmanifest", "./icon.svg", "./assets/MissionLinkLogo.png"];
self.addEventListener("install", event => event.waitUntil(Promise.all([caches.open(CACHE).then(cache => cache.addAll(ASSETS)), self.skipWaiting()])));
self.addEventListener("activate", event => event.waitUntil(caches.keys().then(keys => Promise.all(keys.filter(key => key !== CACHE).map(key => caches.delete(key)))).then(() => self.clients.claim())));
self.addEventListener("fetch", event => {
  if (event.request.method !== "GET") return;
  const url = new URL(event.request.url);
  if (url.origin !== self.location.origin) return;
  // Firmware binaries must always come from the current deployment, never a stale
  // service-worker cache. flasher.js also requests them with cache: "no-store".
  if (url.pathname.includes("/firmware/") || url.pathname.includes("/api/")) return;
  event.respondWith(caches.match(event.request).then(cached => cached || fetch(event.request).then(response => {
    if (response.ok) { const copy = response.clone(); caches.open(CACHE).then(cache => cache.put(event.request, copy)); }
    return response;
  })));
});