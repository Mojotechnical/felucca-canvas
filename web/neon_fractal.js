/* Felucca Canvas: the neon fractal behind the pages. A Julia set at a third of the screen's resolution (the browser
   smooths it up), coloured by its smoothed escape count through a magenta -> violet -> cyan -> lime neon palette,
   dark where it does not escape. animate: c drifts slowly (about 8 frames a second); not with reduced motion. */
function neonFractal(cv, animate) {
  const ctx = cv.getContext("2d");
  const still = !animate || matchMedia("(prefers-reduced-motion: reduce)").matches;
  const STOPS = [[8, 0, 20], [255, 0, 170], [140, 40, 255], [0, 230, 255], [120, 255, 90], [255, 0, 170]];
  const pal = new Uint8ClampedArray(256 * 3);
  for (let i = 0; i < 256; i++) {
    const f = i / 255 * (STOPS.length - 1), k = Math.min(STOPS.length - 2, Math.floor(f)), u = f - k;
    for (let j = 0; j < 3; j++) pal[i * 3 + j] = STOPS[k][j] + (STOPS[k + 1][j] - STOPS[k][j]) * u;
  }
  let t = 1.7, w = 0, h = 0, img = null;
  function size() {
    w = Math.max(64, Math.ceil(innerWidth / 3)); h = Math.max(64, Math.ceil(innerHeight / 3));
    cv.width = w; cv.height = h; img = ctx.createImageData(w, h);
  }
  function draw() {
    const cr = -0.745 + 0.06 * Math.cos(t), ci = 0.113 + 0.06 * Math.sin(t * 0.83);
    const s = 1.9 / Math.min(w, h), d = img.data, MAX = 72;
    for (let y = 0, o = 0; y < h; y++) {
      for (let x = 0; x < w; x++, o += 4) {
        let zx = (x - w / 2) * s, zy = (y - h / 2) * s, n = 0, q = 0;
        for (; n < MAX && (q = zx * zx + zy * zy) < 16; n++) { const nx = zx * zx - zy * zy + cr; zy = 2 * zx * zy + ci; zx = nx; }
        let i = 0;
        if (n < MAX) { const mu = n + 1 - Math.log(Math.log(Math.sqrt(q))) / Math.LN2; i = Math.max(0, Math.min(255, mu * 11)) | 0; }
        else i = 120;
        const glow = n < MAX ? Math.min(1, Math.pow(n / 20, 1.6)) : 0.1;   /* the far field dark, the edges glowing, inside a faint glow */
        d[o] = pal[i * 3] * glow; d[o + 1] = pal[i * 3 + 1] * glow; d[o + 2] = pal[i * 3 + 2] * glow; d[o + 3] = 255;
      }
    }
    ctx.putImageData(img, 0, 0);
  }
  size(); draw();
  addEventListener("resize", () => { size(); draw(); });
  if (!still) setInterval(() => { if (!document.hidden) { t += 0.012; draw(); } }, 120);
}
