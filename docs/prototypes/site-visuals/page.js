/* Review page wiring: live shader targets, loops, reduced motion. */
(function () {
  'use strict';
  var S = window.IV_STUDIES;
  var LOOP = S.loop_seconds;
  var SCALE = S.render_scale;          // render at 0.5x device pixels, CSS scales it up
  var FRAME_MS = 1000 / S.max_fps;     // slow fields: 30 fps is plenty
  var STILL_PH = S.still_phase * Math.PI * 2;
  var reduceMQ = window.matchMedia('(prefers-reduced-motion: reduce)');
  var userPaused = false;
  function stillMode() { return reduceMQ.matches || userPaused; }

  var r = new window.IVRenderer();
  var targets = [];
  var visible = new Set();

  document.querySelectorAll('[data-live]').forEach(function (el) {
    var t = { el: el, canvas: el.querySelector('canvas'), key: el.dataset.study + '-' + el.dataset.variant,
      light: el.dataset.mode === 'light' ? 1 : 0, amt: parseFloat(el.dataset.amt || '1'), ctx: null, drawn: false };
    targets.push(t);
  });

  function sizeOf(t) {
    var rect = t.el.getBoundingClientRect();
    var dpr = window.devicePixelRatio || 1;
    var w = Math.max(2, Math.min(1800, Math.round(rect.width * dpr * SCALE)));
    var h = Math.max(2, Math.min(1400, Math.round(rect.height * dpr * SCALE)));
    return [w, h];
  }

  function drawTarget(t, ph) {
    var wh = sizeOf(t);
    if (t.canvas.width !== wh[0] || t.canvas.height !== wh[1]) {
      t.canvas.width = wh[0];
      t.canvas.height = wh[1];
    }
    if (!t.ctx) t.ctx = t.canvas.getContext('2d', { alpha: false });
    r.draw(t.key, S.src[t.key], wh[0], wh[1], ph, t.light, t.amt, false);
    r.blit(t.ctx, wh[0], wh[1]);
    if (!t.drawn) { t.drawn = true; t.el.classList.add('is-live'); }
  }

  var raf = 0;
  var last = 0;
  function tick(now) {
    raf = 0;
    if (stillMode()) return;
    raf = requestAnimationFrame(tick);
    if (now - last < FRAME_MS - 2) return;
    last = now;
    var ph = ((now / 1000) % LOOP) / LOOP * Math.PI * 2;
    visible.forEach(function (t) { drawTarget(t, ph); });
  }
  function start() {
    if (!r.ok) return;
    if (stillMode()) {
      visible.forEach(function (t) { drawTarget(t, STILL_PH); });
      return;
    }
    if (!raf) raf = requestAnimationFrame(tick);
  }

  if (r.ok) {
    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (en) {
        var t = targets.find(function (x) { return x.el === en.target; });
        if (en.isIntersecting) {
          visible.add(t);
          if (stillMode()) drawTarget(t, STILL_PH);
        } else {
          visible.delete(t);
        }
      });
      start();
    }, { rootMargin: '100px 0px' });
    targets.forEach(function (t) { io.observe(t.el); });
    var rz = 0;
    window.addEventListener('resize', function () {
      clearTimeout(rz);
      rz = setTimeout(function () { if (stillMode()) visible.forEach(function (t) { drawTarget(t, STILL_PH); }); }, 150);
    });
  } else {
    document.documentElement.classList.add('no-webgl');
  }

  // Loops: play only while on screen and motion is allowed; otherwise the poster (the still) shows.
  var vids = Array.prototype.slice.call(document.querySelectorAll('video[data-loop]'));
  var vidVisible = new Set();
  function syncVideos() {
    vids.forEach(function (v) {
      if (stillMode()) {
        v.setAttribute('controls', '');
        if (!v.paused) v.pause();
      } else {
        v.removeAttribute('controls');
        if (vidVisible.has(v)) { var pr = v.play(); if (pr && pr.catch) pr.catch(function () {}); } else if (!v.paused) v.pause();
      }
    });
  }
  var vio = new IntersectionObserver(function (entries) {
    entries.forEach(function (en) { if (en.isIntersecting) vidVisible.add(en.target); else vidVisible.delete(en.target); });
    syncVideos();
  }, { threshold: 0.1 });
  vids.forEach(function (v) { vio.observe(v); });

  // Motion switch in the index (a viewing aid, not a choice)
  var btn = document.getElementById('motion-toggle');
  function label() {
    if (!btn) return;
    if (reduceMQ.matches) { btn.textContent = 'Motion off (your system setting)'; btn.disabled = true; return; }
    btn.disabled = false;
    btn.textContent = userPaused ? 'Play motion' : 'Pause motion';
    btn.setAttribute('aria-pressed', userPaused ? 'true' : 'false');
  }
  if (btn) btn.addEventListener('click', function () { userPaused = !userPaused; label(); syncVideos(); start(); });
  reduceMQ.addEventListener('change', function () { label(); syncVideos(); start(); });
  label();
  syncVideos();
})();
