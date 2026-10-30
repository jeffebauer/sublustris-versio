/* One shared WebGL context draws every study, then copies each frame into that
   target's own 2D canvas. Browsers cap live WebGL contexts at about 16, and this
   page has 30 canvases, so one context is the only safe way. */
(function (global) {
  'use strict';
  var VS = 'attribute vec2 a;void main(){gl_Position=vec4(a,0.0,1.0);}';

  function Renderer(opts) {
    opts = opts || {};
    this.canvas = opts.canvas || document.createElement('canvas');
    this.canvas.width = 2;
    this.canvas.height = 2;
    var attrs = { antialias: false, alpha: false, depth: false, stencil: false,
      premultipliedAlpha: false, preserveDrawingBuffer: !!opts.preserve,
      powerPreference: 'low-power' };
    var gl = null;
    try { gl = this.canvas.getContext('webgl', attrs); } catch (e) { gl = null; }
    this.gl = gl;
    this.ok = !!gl;
    this.programs = {};
    if (!gl) return;
    var buf = gl.createBuffer();
    this.buf = buf;
    gl.bindBuffer(gl.ARRAY_BUFFER, buf);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 3, -1, -1, 3]), gl.STATIC_DRAW);
  }

  Renderer.prototype.compile = function (type, src) {
    var gl = this.gl;
    var s = gl.createShader(type);
    gl.shaderSource(s, src);
    gl.compileShader(s);
    if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) {
      throw new Error('shader: ' + gl.getShaderInfoLog(s));
    }
    return s;
  };

  // key: e.g. "diffusion-0"; src: full fragment source
  Renderer.prototype.program = function (key, src) {
    if (this.programs[key]) return this.programs[key];
    var gl = this.gl;
    var p = gl.createProgram();
    gl.attachShader(p, this.compile(gl.VERTEX_SHADER, VS));
    gl.attachShader(p, this.compile(gl.FRAGMENT_SHADER, src));
    gl.bindAttribLocation(p, 0, 'a');
    gl.linkProgram(p);
    if (!gl.getProgramParameter(p, gl.LINK_STATUS)) throw new Error('link: ' + gl.getProgramInfoLog(p));
    var U = function (n) { return gl.getUniformLocation(p, n); };
    var prog = { p: p, res: U('u_res'), ph: U('u_ph'), t: U('u_t'), loopT: U('u_loopT'), live: U('u_live'), amt: U('u_amt') };
    this.programs[key] = prog;
    return prog;
  };

  // u: { ph: loop phase 0..2pi, t: seconds (live), loopT: loop seconds, live: 0|1, amt }
  // exact: resize the GL canvas to w x h (capture); otherwise it only grows (shared use)
  Renderer.prototype.draw = function (key, src, w, h, u, exact) {
    var gl = this.gl;
    var c = this.canvas;
    if (exact) {
      if (c.width !== w || c.height !== h) { c.width = w; c.height = h; }
    } else if (c.width < w || c.height < h) {
      c.width = Math.max(c.width, w);
      c.height = Math.max(c.height, h);
    }
    var prog = this.program(key, src);
    gl.viewport(0, 0, w, h);
    gl.useProgram(prog.p);
    gl.bindBuffer(gl.ARRAY_BUFFER, this.buf);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
    gl.uniform2f(prog.res, w, h);
    gl.uniform1f(prog.ph, u.ph);
    gl.uniform1f(prog.t, u.t || 0);
    gl.uniform1f(prog.loopT, u.loopT);
    gl.uniform1f(prog.live, u.live ? 1 : 0);
    gl.uniform1f(prog.amt, u.amt);
    gl.drawArrays(gl.TRIANGLES, 0, 3);
  };

  // copy the last draw (bottom-left w x h of the GL canvas) into a 2D context
  Renderer.prototype.blit = function (ctx, w, h) {
    ctx.drawImage(this.canvas, 0, this.canvas.height - h, w, h, 0, 0, w, h);
  };

  global.IVRenderer = Renderer;
})(window);
