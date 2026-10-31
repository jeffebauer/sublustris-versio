/* Review page wiring: live shader targets, loops, reduced motion. */
(function () {
  'use strict';
  var S = window.IV_STUDIES;
  var SCALE = S.render_scale;          // render at 0.5x device pixels, CSS scales it up
  var FRAME_MS = 1000 / S.max_fps;     // slow fields: 30 fps is plenty
  var STILL_PH = S.still_phase * Math.PI * 2;
  var reduceMQ = window.matchMedia('(prefers-reduced-motion: reduce)');
  var userPaused = false;
  function stillMode() { return reduceMQ.matches || userPaused; }

  var r = new window.IVRenderer();
  var targets = [];
  var visible = new Set();
  var t0 = performance.now();
  var pausedAt = 0;      // live time keeps running forward only while motion is on
  var offset = 0;

  document.querySelectorAll('[data-live]').forEach(function (el) {
    var o = S.options[el.dataset.option];
    targets.push({ el: el, canvas: el.querySelector('canvas'), prog: o.prog, loopT: o.loop,
      amt: parseFloat(el.dataset.amt || '1') * o.gain, ctx: null, drawn: false });
  });

  function sizeOf(t) {
    var rect = t.el.getBoundingClientRect();
    var dpr = window.devicePixelRatio || 1;
    return [Math.max(2, Math.min(1800, Math.round(rect.width * dpr * SCALE))),
            Math.max(2, Math.min(1400, Math.round(rect.height * dpr * SCALE)))];
  }

  function drawTarget(t, now) {
    var wh = sizeOf(t);
    if (t.canvas.width !== wh[0] || t.canvas.height !== wh[1]) { t.canvas.width = wh[0]; t.canvas.height = wh[1]; }
    if (!t.ctx) t.ctx = t.canvas.getContext('2d', { alpha: false });
    var u;
    if (now === null) {
      u = { ph: STILL_PH, t: 0, loopT: t.loopT, live: 0, amt: t.amt };     // the same frame as the still image
    } else {
      var sec = S.still_phase * t.loopT + (now - t0) / 1000 - offset;   // live begins at the still frame
      u = { ph: (sec % t.loopT) / t.loopT * Math.PI * 2, t: sec, loopT: t.loopT, live: 1, amt: t.amt };
    }
    r.draw(t.prog, S.src[t.prog], wh[0], wh[1], u, false);
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
    visible.forEach(function (t) { drawTarget(t, now); });
  }
  function start() {
    if (!r.ok) return;
    if (stillMode()) {
      if (!pausedAt) pausedAt = performance.now();
      visible.forEach(function (t) { drawTarget(t, null); });
      return;
    }
    if (pausedAt) { offset += (performance.now() - pausedAt) / 1000; pausedAt = 0; }
    if (!raf) raf = requestAnimationFrame(tick);
  }

  if (r.ok) {
    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (en) {
        var t = targets.find(function (x) { return x.el === en.target; });
        if (en.isIntersecting) {
          visible.add(t);
          if (stillMode()) drawTarget(t, null);
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
      rz = setTimeout(function () { if (stillMode()) visible.forEach(function (t) { drawTarget(t, null); }); }, 150);
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
