/*
 * CRYPTLINGS: chibi cryptid sprites (vector SVG, 100 x 100 viewBox)
 *
 * window.CRYPTID_SPRITES[species][form](opts) returns a STRING of SVG inner markup.
 * species: mothman, bigfoot, nessie, nightcrawler
 * forms:   egg, whisper, rumor, sighting, legend, blurry
 * opts:    { mood: 'happy'|'neutral'|'sad'|'sleep', blink: boolean, t: seconds (optional) }
 *
 * Creatures stand on a ground line near y=92, centered at x=50.
 * Every <defs> id is prefixed per species and form (for example "mm-legend-glow").
 * Art rules: bold simple shapes, dark soft outline, flat fills with at most one shade,
 * big eyes with a white highlight. Built to survive rasterizing to 16-color bitmaps.
 */
(function () {
  'use strict';

  var OL = '#1a1622';
  var SW = 2.2;

  // ---------- tiny string helpers ----------
  function n(v) { return Math.round(v * 10) / 10; }
  function pt(x, y) { return n(x) + ',' + n(y); }
  function strokeAttr(ol, sw) {
    return ol ? ' stroke="' + ol + '" stroke-width="' + n(sw || SW) + '" stroke-linejoin="round" stroke-linecap="round"' : '';
  }
  function C(cx, cy, r, fill, ol, sw, x) {
    return '<circle cx="' + n(cx) + '" cy="' + n(cy) + '" r="' + n(r) + '" fill="' + (fill || 'none') + '"' + strokeAttr(ol, sw) + (x || '') + '/>';
  }
  function E(cx, cy, rx, ry, fill, ol, sw, x) {
    return '<ellipse cx="' + n(cx) + '" cy="' + n(cy) + '" rx="' + n(rx) + '" ry="' + n(ry) + '" fill="' + (fill || 'none') + '"' + strokeAttr(ol, sw) + (x || '') + '/>';
  }
  function P(d, fill, ol, sw, x) {
    return '<path d="' + d + '" fill="' + (fill || 'none') + '"' + strokeAttr(ol, sw) + (x || '') + '/>';
  }
  function G(inner, x) { return '<g' + (x || '') + '>' + inner + '</g>'; }
  function rot(a, cx, cy) { return ' transform="rotate(' + n(a) + ' ' + n(cx) + ' ' + n(cy) + ')"'; }
  var MIRROR = ' transform="translate(100 0) scale(-1 1)"';
  // an outlined "tube" (limbs, necks, pant legs): outline stroke under a fill stroke
  function tube(d, w, fill, ol) { return P(d, null, ol, w + 2 * SW) + P(d, null, fill, w); }
  function shadow(rx) { return E(50, 92.6, rx, 2.4, '#000000', null, null, ' opacity="0.3"'); }

  // ---------- shape generators ----------
  // soft scalloped "fuzzy" ellipse
  function fuzzD(cx, cy, rx, ry, count, amp, rot0) {
    var st = Math.PI * 2 / count, d = '', a0 = rot0 || 0;
    for (var i = 0; i <= count; i++) {
      var a = a0 + i * st, px = cx + Math.cos(a) * rx, py = cy + Math.sin(a) * ry;
      if (i === 0) { d = 'M' + pt(px, py); continue; }
      var am = a - st / 2;
      d += 'Q' + pt(cx + Math.cos(am) * rx * (1 + amp), cy + Math.sin(am) * ry * (1 + amp)) + ' ' + pt(px, py);
    }
    return d + 'Z';
  }
  // shaggy ellipse with soft swept tufts
  function tuftD(cx, cy, rx, ry, count, amp, rot0) {
    var st = Math.PI * 2 / count, d = '', a0 = rot0 || 0;
    function at(a, k) { return pt(cx + Math.cos(a) * rx * k, cy + Math.sin(a) * ry * k); }
    for (var i = 0; i < count; i++) {
      var a = a0 + i * st;
      if (i === 0) d = 'M' + at(a, 1);
      d += 'Q' + at(a + st * 0.25, 1 + amp * 0.75) + ' ' + at(a + st * 0.7, 1 + amp);
      d += 'Q' + at(a + st * 0.75, 1 + amp * 0.3) + ' ' + at(a + st, 1);
    }
    return d + 'Z';
  }
  function eggD(cx, cy, rx, ry) {
    var T = pt(cx, cy - ry), R = pt(cx + rx, cy + ry * 0.15), B = pt(cx, cy + ry), L = pt(cx - rx, cy + ry * 0.15);
    return 'M' + T +
      'C' + pt(cx + rx * 0.62, cy - ry) + ' ' + pt(cx + rx, cy - ry * 0.45) + ' ' + R +
      'C' + pt(cx + rx, cy + ry * 0.72) + ' ' + pt(cx + rx * 0.55, cy + ry) + ' ' + B +
      'C' + pt(cx - rx * 0.55, cy + ry) + ' ' + pt(cx - rx, cy + ry * 0.72) + ' ' + L +
      'C' + pt(cx - rx, cy - ry * 0.45) + ' ' + pt(cx - rx * 0.62, cy - ry) + ' ' + T + 'Z';
  }
  // jagged eggshell cap sitting on a head whose top is at y
  function shellCap(cx, y, w, fill, ol, tilt) {
    var d = 'M' + pt(cx - w, y + w * 0.32) + 'Q' + pt(cx, y - w * 0.75) + ' ' + pt(cx + w, y + w * 0.32) +
      'L' + pt(cx + w * 0.62, y + w * 0.12) + 'L' + pt(cx + w * 0.32, y + w * 0.45) + 'L' + pt(cx, y + w * 0.14) +
      'L' + pt(cx - w * 0.34, y + w * 0.45) + 'L' + pt(cx - w * 0.64, y + w * 0.14) + 'Z';
    return P(d, fill, ol, 1.8, rot(tilt || -12, cx, y));
  }
  // leaf/feather shape between base and tip
  function leafD(bx, by, tx, ty, w) {
    var mx = (bx + tx) / 2, my = (by + ty) / 2, dx = tx - bx, dy = ty - by, L = Math.sqrt(dx * dx + dy * dy) || 1;
    var px = -dy / L * w, py = dx / L * w;
    return 'M' + pt(bx, by) + 'Q' + pt(mx + px, my + py) + ' ' + pt(tx, ty) + 'Q' + pt(mx - px, my - py) + ' ' + pt(bx, by) + 'Z';
  }
  function dropD(x, y, s) { // teardrop, point up, (x,y) is the round bottom center
    return 'M' + pt(x, y - s * 1.6) + 'C' + pt(x + s * 0.4, y - s * 0.9) + ' ' + pt(x + s, y - s * 0.4) + ' ' + pt(x + s, y) +
      'A' + n(s) + ' ' + n(s) + ' 0 0 1 ' + pt(x - s, y) +
      'C' + pt(x - s, y - s * 0.4) + ' ' + pt(x - s * 0.4, y - s * 0.9) + ' ' + pt(x, y - s * 1.6) + 'Z';
  }
  // one-tone shade crescent along the bottom of a circle
  function crescentD(cx, cy, r) {
    return 'M' + pt(cx - r * 0.8, cy + r * 0.6) + 'A' + n(r) + ' ' + n(r) + ' 0 0 0 ' + pt(cx + r * 0.8, cy + r * 0.6) +
      'Q' + pt(cx, cy + r * 0.72) + ' ' + pt(cx - r * 0.8, cy + r * 0.6) + 'Z';
  }

  // ---------- color helpers (for the BLURRY form) ----------
  function rgb(h) { h = h.replace('#', ''); return [parseInt(h.substr(0, 2), 16), parseInt(h.substr(2, 2), 16), parseInt(h.substr(4, 2), 16)]; }
  function hex(c) { return '#' + c.map(function (v) { v = Math.max(0, Math.min(255, Math.round(v))); return (v < 16 ? '0' : '') + v.toString(16); }).join(''); }
  function washed(h, k) {
    var c = rgb(h), l = 0.3 * c[0] + 0.59 * c[1] + 0.11 * c[2], tint = [152, 156, 166];
    return hex(c.map(function (v, i) { var g = v * (1 - k) + l * k; return g * 0.78 + tint[i] * 0.22; }));
  }
  function blurPal(pal) {
    var out = {};
    for (var k in pal) out[k] = washed(pal[k], k === 'eye' ? 0.35 : 0.72);
    return out;
  }

  var COMMON = { ol: OL, hi: '#FFFFFF', mouth: '#3A1E2A', tongue: '#E77C8A', blush: '#F29BA8', tear: '#A9D8F2' };
  function pal(o) { var p = {}; for (var k in COMMON) p[k] = COMMON[k]; for (var j in o) p[j] = o[j]; return p; }

  var PAL = {
    mothman: pal({
      body: '#8E8073', bodyS: '#74675C', ruff: '#B8A88A', ruffS: '#9E8E70',
      wing: '#6B6475', wingS: '#57505F', pat: '#B8A88A', spot: '#2E2A36',
      eye: '#FF4A3D', leg: '#3D3542', cocoon: '#A8987E', cocoonS: '#8E7F66', silk: '#D9CDB3'
    }),
    bigfoot: pal({
      fur: '#8A5A3B', furS: '#6D452C', face: '#C9A27E', belly: '#A87A55', foot: '#B98D66', nose: '#4A2E20',
      leaf: '#7FB069', leafS: '#5B8A4A', egg: '#7E5D40', eggS: '#664A33', moss: '#7FB069', mossS: '#5E8F4C', print: '#3E2A1E'
    }),
    nessie: pal({
      body: '#3E8E7E', bodyS: '#2F7064', belly: '#A8D5BA', spot: '#2A6559',
      water: '#4FA3A5', waterS: '#3C8689', waterHi: '#A3DCD8', egg: '#6FA59A', speck: '#2F5F57', speckL: '#CBE6D8'
    }),
    nightcrawler: pal({ body: '#EDEDF2', shade: '#C9C3E0', bow: '#8E7CC3', foot: '#C9C3E0' })
  };
  var BPAL = {};
  for (var sp in PAL) BPAL[sp] = blurPal(PAL[sp]);

  function glowDef(id, color, strength) {
    var s = strength || 0.7;
    return '<defs><radialGradient id="' + id + '">' +
      '<stop offset="0" stop-color="' + color + '" stop-opacity="' + s + '"/>' +
      '<stop offset="0.5" stop-color="' + color + '" stop-opacity="' + n(s * 0.4) + '"/>' +
      '<stop offset="1" stop-color="' + color + '" stop-opacity="0"/></radialGradient></defs>';
  }

  // ---------- faces ----------
  // f: {x, y, gap, er, kind:'big'|'glow'|'dot', pal, mood, blink, glowId, blurry}
  function eyes(f) {
    var s = '', p = f.pal, m = f.mood, r = f.er, lw = Math.max(1.6, Math.min(2.6, r * 0.42));
    for (var i = 0; i < 2; i++) {
      var ex = f.x + (i ? f.gap : -f.gap), ey = f.y, side = i ? 1 : -1;
      if (f.blurry) {
        if (f.blink || m === 'sleep') s += P('M' + pt(ex - r * 0.6, ey) + 'L' + pt(ex + r * 0.6, ey), null, p.ol, 1.8);
        else {
          if (f.kind === 'glow') s += C(ex, ey, r * 1.1, p.eye, null, null, ' opacity="0.45"');
          s += C(ex, ey + r * 0.15, r * 0.5, f.kind === 'glow' ? p.eye : p.ol);
        }
        continue;
      }
      if (f.blink) {
        s += P('M' + pt(ex - r * 0.8, ey + r * 0.1) + 'L' + pt(ex + r * 0.8, ey + r * 0.1), null, p.ol, lw);
      } else if (m === 'sleep') {
        s += P('M' + pt(ex - r * 0.8, ey) + 'Q' + pt(ex, ey + r * 0.95) + ' ' + pt(ex + r * 0.8, ey), null, p.ol, lw);
      } else if (m === 'happy') {
        if (f.kind === 'glow') s += C(ex, ey, r * 1.7, 'url(#' + f.glowId + ')');
        s += P('M' + pt(ex - r * 0.85, ey + r * 0.4) + 'Q' + pt(ex, ey - r * 1.05) + ' ' + pt(ex + r * 0.85, ey + r * 0.4),
          null, f.kind === 'glow' ? p.eye : p.ol, f.kind === 'glow' ? lw * 1.35 : lw);
      } else {
        if (f.kind === 'glow') {
          s += C(ex, ey, r * 2, 'url(#' + f.glowId + ')');
          s += E(ex, ey, r * 0.92, r, p.eye, p.ol, 1.6);
        } else if (f.kind === 'dot') {
          s += C(ex, ey, r, p.ol);
        } else {
          s += E(ex, ey, r * 0.84, r, p.ol);
        }
        var hk = f.kind === 'dot' ? 0.42 : 0.34;
        s += C(ex - r * 0.3, ey - r * 0.36, r * hk, p.hi);
        if (f.kind !== 'dot') s += C(ex + r * 0.3, ey + r * 0.38, r * 0.15, p.hi);
        if (m === 'sad') {
          var bw = Math.max(r, 2.6);
          s += P('M' + pt(ex + side * bw * 0.95, ey - r - bw * 0.25) + 'L' + pt(ex - side * bw * 0.55, ey - r - bw * 0.75), null, p.ol, Math.max(1.5, lw * 0.8));
          if (i === 1) s += P(dropD(ex + r * 0.55, ey + r + bw * 0.75, Math.max(1.3, r * 0.3)), p.tear, p.ol, 1);
        }
      }
    }
    return s;
  }
  // m: {x, y, w, pal, mood, fang, blurry}
  function mouth(m) {
    var p = m.pal, x = m.x, y = m.y, w = m.w, mood = m.mood, s = '';
    if (m.blurry) {
      return P('M' + pt(x - w * 0.5, y + w * 0.35) + 'Q' + pt(x, y - w * 0.1) + ' ' + pt(x + w * 0.5, y + w * 0.35), null, p.ol, 1.6);
    }
    if (mood === 'happy') {
      s += P('M' + pt(x - w, y) + 'Q' + pt(x, y + w * 1.6) + ' ' + pt(x + w, y) + 'Z', p.mouth, p.ol, 1.5);
      s += E(x, y + w * 0.5, w * 0.42, w * 0.2, p.tongue);
      if (m.fang) s += P('M' + pt(x - w * 0.62, y + 0.5) + 'L' + pt(x - w * 0.42, y + w * 0.5) + 'L' + pt(x - w * 0.22, y + 0.5) + 'Z', p.hi) +
        P('M' + pt(x + w * 0.62, y + 0.5) + 'L' + pt(x + w * 0.42, y + w * 0.5) + 'L' + pt(x + w * 0.22, y + 0.5) + 'Z', p.hi);
    } else if (mood === 'sad') {
      s += P('M' + pt(x - w * 0.6, y + w * 0.45) + 'Q' + pt(x, y - w * 0.25) + ' ' + pt(x + w * 0.6, y + w * 0.45), null, p.ol, 1.6);
    } else if (mood === 'sleep') {
      s += E(x, y + w * 0.2, w * 0.28, w * 0.22, p.mouth, p.ol, 1.2);
    } else {
      s += P('M' + pt(x - w * 0.55, y + w * 0.05) + 'Q' + pt(x, y + w * 0.5) + ' ' + pt(x + w * 0.55, y + w * 0.05), null, p.ol, 1.6);
      if (m.fang) s += P('M' + pt(x + w * 0.12, y + w * 0.22) + 'L' + pt(x + w * 0.27, y + w * 0.6) + 'L' + pt(x + w * 0.42, y + w * 0.14) + 'Z', p.hi, p.ol, 0.8);
    }
    return s;
  }
  function blush(x, y, gap, er, p, mood, blurry) {
    if (blurry || (mood !== 'happy')) return '';
    return E(x - gap - er * 0.3, y, er * 0.6, er * 0.33, p.blush, null, null, ' opacity="0.8"') +
      E(x + gap + er * 0.3, y, er * 0.6, er * 0.33, p.blush, null, null, ' opacity="0.8"');
  }
  function moodOf(o) { o = o || {}; return { mood: o.mood || 'neutral', blink: !!o.blink, t: +o.t || 0 }; }

  // ---------- shared EGG wrapper ----------
  function eggWrap(inner, o, bottomY, shadowRx) {
    var tr = '';
    if (o.mood === 'sad') tr = rot(-12, 50, bottomY);
    else if (o.mood === 'happy') tr = rot(Math.sin(o.t * 5) * 4, 50, bottomY);
    else if (o.mood === 'sleep') tr = ' transform="translate(0 ' + n(bottomY * 0.05) + ') scale(1 0.95)"';
    return shadow(shadowRx) + G(inner, tr);
  }
  function crack(cx, cy, rx, ry, ol) {
    return P('M' + pt(cx - rx * 0.62, cy - ry * 0.3) + 'L' + pt(cx - rx * 0.3, cy - ry * 0.46) + 'L' + pt(cx - rx * 0.05, cy - ry * 0.28) +
      'L' + pt(cx + rx * 0.22, cy - ry * 0.48) + 'L' + pt(cx + rx * 0.5, cy - ry * 0.34), null, ol, 1.7);
  }

  // ---------- BLURRY wrapper ----------
  var GRAIN = [[14, 20], [83, 15], [30, 70], [72, 48], [20, 44], [88, 80], [60, 12], [40, 30], [10, 85], [66, 76], [47, 55], [92, 34]];
  function blurryWrap(prefix, inner) {
    var s = '<defs><filter id="' + prefix + '-soft" x="-30%" y="-30%" width="160%" height="160%"><feGaussianBlur stdDeviation="1.5"/></filter></defs>';
    var fit = ' transform="translate(50 92) scale(0.93) translate(-50 -92)"';
    s += G(G(inner, ' transform="translate(3.6 -1.8)" opacity="0.4"') + G(inner), fit + ' filter="url(#' + prefix + '-soft)"');
    for (var i = 0; i < GRAIN.length; i++) {
      s += C(GRAIN[i][0], GRAIN[i][1], i % 3 ? 0.6 : 0.9, i % 2 ? '#C9CDD6' : '#5A5E68', null, null, ' opacity="0.45"');
    }
    return s;
  }

  // =====================================================================
  // MOTHMAN
  // =====================================================================
  function mmWing(sx, sy, s, h, p, simple) {
    var hind = 'M' + pt(sx, sy + h * 0.05) + 'C' + pt(sx - s * 0.55, sy) + ' ' + pt(sx - s * 0.82, sy + h * 0.5) + ' ' + pt(sx - s * 0.56, sy + h * 0.74) +
      'C' + pt(sx - s * 0.3, sy + h * 0.92) + ' ' + pt(sx - s * 0.05, sy + h * 0.55) + ' ' + pt(sx, sy + h * 0.35) + 'Z';
    var fore = 'M' + pt(sx, sy) + 'C' + pt(sx - s * 0.25, sy - h * 0.95) + ' ' + pt(sx - s * 0.85, sy - h * 1.05) + ' ' + pt(sx - s, sy - h * 0.7) +
      'C' + pt(sx - s * 1.05, sy - h * 0.3) + ' ' + pt(sx - s * 0.6, sy + h * 0.02) + ' ' + pt(sx, sy + h * 0.12) + 'Z';
    var out = P(hind, p.wingS, p.ol) + P(fore, p.wing, p.ol);
    if (simple) return out;
    out += P('M' + pt(sx - s * 0.3, sy - h * 0.72) + 'Q' + pt(sx - s * 0.82, sy - h * 0.86) + ' ' + pt(sx - s * 0.9, sy - h * 0.5), null, p.pat, Math.max(1.6, h * 0.09));
    out += C(sx - s * 0.52, sy - h * 0.36, h * 0.17, p.pat, p.ol, 1.4) + C(sx - s * 0.52, sy - h * 0.36, h * 0.075, p.spot);
    out += C(sx - s * 0.5, sy + h * 0.48, h * 0.1, p.pat);
    return out;
  }
  function mmDraw(Q, o, p, id, blurry) {
    var s = '', hx = Q.hx, hy = Q.hy, hr = Q.hr;
    var flap = blurry ? 0 : Math.sin(o.t * 5) * (Q.flap || 3);
    var gid = id + '-glow';
    s += glowDef(gid, p.eye, Q.glow || 0.7);
    if (!blurry) s += shadow(Q.shw);
    // antennae
    for (var i = -1; i <= 1; i += 2) {
      var bx = hx + i * hr * 0.42, by = hy - hr * 0.78;
      var tx = hx + i * Q.ax, ty = Q.ty + (Q.bent && i > 0 ? 5 : 0);
      if (Q.bent && i > 0) tx += 3;
      s += P('M' + pt(bx, by) + 'L' + pt(tx, ty), null, p.ol, 2);
      s += P(leafD(bx + (tx - bx) * 0.12, by + (ty - by) * 0.12, tx, ty, Q.aw), p.ruff, p.ol, 1.6);
      s += P('M' + pt(bx + (tx - bx) * 0.3, by + (ty - by) * 0.3) + 'L' + pt(tx, ty), null, p.ruffS, 1);
    }
    // wings
    var wl = mmWing(Q.sx, Q.sy, Q.ws, Q.wh, p, blurry);
    s += G(wl, rot(flap + (Q.wr || 0), Q.sx, Q.sy));
    s += G(G(wl, rot(flap + (Q.wr2 !== undefined ? Q.wr2 : (Q.wr || 0)), Q.sx, Q.sy)), MIRROR);
    // legs and feet
    for (var j = -1; j <= 1; j += 2) {
      s += tube('M' + pt(50 + j * Q.lx, Q.ly) + 'L' + pt(50 + j * (Q.lx + 0.5), 89.5), Q.lw, p.leg, p.ol);
      s += E(50 + j * (Q.lx + 1.5), 90.6, Q.lw * 0.95, Q.lw * 0.55, p.leg, p.ol, 1.8);
    }
    // body
    s += P(fuzzD(50, Q.by, Q.brx, Q.bry, 14, 0.1, 0.2), p.body, p.ol);
    if (!blurry) s += P(crescentD(50, Q.by, Math.min(Q.brx, Q.bry) * 0.92), p.bodyS);
    // ruff collar
    s += P(fuzzD(50, Q.ry, Q.rrx, Q.rry, 14, 0.42, 0.1), p.ruff, p.ol);
    // head
    s += P(fuzzD(hx, hy, hr, hr * 0.95, 16, 0.08, 0), p.body, p.ol);
    if (!blurry) s += P(fuzzD(hx, hy - hr * 0.55, hr * 0.42, hr * 0.22, 7, 0.35, 3.3), p.ruff);
    // face
    var er = Q.er, ey = hy + hr * 0.06;
    s += eyes({ x: hx, y: ey, gap: Q.eg, er: er, kind: 'glow', pal: p, mood: o.mood, blink: o.blink, glowId: gid, blurry: blurry });
    s += blush(hx, ey + er * 1.15, Q.eg, er, p, o.mood, blurry);
    s += mouth({ x: hx, y: hy + hr * 0.6, w: Q.mw, pal: p, mood: o.mood, fang: Q.fang, blurry: blurry });
    if (Q.shell) s += shellCap(hx + 2, hy - hr * 0.92, hr * 0.62, p.cocoon, p.ol, 14);
    return s;
  }
  var MM = {
    whisper: { hx: 50, hy: 70, hr: 15, by: 84, brx: 10, bry: 6.5, ry: 80, rrx: 11, rry: 3, sx: 45, sy: 80, ws: 15, wh: 12, lx: 4, ly: 86, lw: 3,
      ax: 9, ty: 49, aw: 2.4, er: 5, eg: 6, mw: 3, shw: 14, shell: true, flap: 4 },
    rumor: { hx: 50, hy: 52, hr: 16.5, by: 76, brx: 12.5, bry: 10.5, ry: 66, rrx: 16, rry: 4.5, sx: 45, sy: 72, ws: 22, wh: 18, lx: 5, ly: 84, lw: 3.4,
      ax: 12, ty: 30, aw: 3, er: 5.6, eg: 6.6, mw: 3.4, shw: 18, flap: 4 },
    sighting: { hx: 50, hy: 37, hr: 16, by: 62, brx: 11.5, bry: 13.5, ry: 51, rrx: 15, rry: 4.5, sx: 45, sy: 58, ws: 31, wh: 26, lx: 4.5, ly: 72, lw: 3,
      ax: 14, ty: 12, aw: 3.2, er: 5.2, eg: 6.6, mw: 3.2, shw: 20, wr: -4, wr2: 16, bent: true, flap: 3 },
    legend: { hx: 50, hy: 34, hr: 18.5, by: 66, brx: 16, bry: 15.5, ry: 51, rrx: 21, rry: 6.5, sx: 44, sy: 57, ws: 39, wh: 30, lx: 6, ly: 80, lw: 4,
      ax: 16, ty: 6, aw: 3.8, er: 6.2, eg: 7.6, mw: 4, shw: 22, fang: true, glow: 0.85, flap: 4 }
  };
  function mmEgg(o) {
    var p = PAL.mothman, cx = 50, cy = 64, rx = 19, ry = 26, s = '';
    s += P('M' + pt(50, 38) + 'Q' + pt(48, 33) + ' ' + pt(51, 29), null, p.silk, 1.6);
    s += P(fuzzD(cx, cy, rx, ry, 22, 0.06, 0), p.cocoon, p.ol);
    s += P(crescentD(cx, cy + 6, 18.5), p.cocoonS);
    s += P('M' + pt(cx - 17, cy - 10) + 'Q' + pt(cx, cy - 4) + ' ' + pt(cx + 17, cy - 13), null, p.silk, 1.5);
    s += P('M' + pt(cx - 18, cy + 10) + 'Q' + pt(cx, cy + 15) + ' ' + pt(cx + 18, cy + 7), null, p.silk, 1.5);
    var wingMark = P('M' + pt(cx, cy - 1) + 'Q' + pt(cx - 8, cy - 11) + ' ' + pt(cx - 13, cy - 5) + 'Q' + pt(cx - 11, cy + 2) + ' ' + pt(cx, cy + 2) + 'Z', p.wing, p.ol, 1.4) +
      P('M' + pt(cx, cy + 2) + 'Q' + pt(cx - 8, cy + 2) + ' ' + pt(cx - 8, cy + 8) + 'Q' + pt(cx - 3, cy + 11) + ' ' + pt(cx, cy + 5) + 'Z', p.wingS, p.ol, 1.4) +
      C(cx - 7.5, cy - 4, 1.8, p.pat);
    s += wingMark + G(wingMark, MIRROR);
    s += C(cx - 6, cy - 16, 1.8, p.wing) + C(cx + 7, cy - 17, 1.5, p.wing) + C(cx - 2, cy + 17, 1.7, p.wing) + C(cx + 9, cy + 13, 1.3, p.wing);
    if (o.mood === 'happy') s += crack(cx, cy, rx, ry, p.ol);
    return eggWrap(s, o, 90, 16);
  }

  // =====================================================================
  // BIGFOOT
  // =====================================================================
  function bfFoot(x, y, rx, ry, a, p, simple) {
    var s = E(x, y, rx, ry, p.foot, p.ol);
    if (!simple) {
      for (var k = -1; k <= 1; k++) s += C(x + k * rx * 0.42, y - ry * 0.55, ry * 0.42, p.foot, p.ol, 1.4);
      s += E(x, y + ry * 0.15, rx * 0.55, ry * 0.4, p.face, null, null, ' opacity="0.9"');
    }
    return G(s, rot(a, x, y));
  }
  function bfDraw(Q, o, p, id, blurry) {
    var s = '', hx = Q.hx, hy = Q.hy, hr = Q.hr;
    if (!blurry) s += shadow(Q.shw);
    // arms (behind body)
    for (var i = 0; i < Q.arms.length; i++) {
      var A = Q.arms[i];
      s += tube('M' + pt(A[0], A[1]) + 'Q' + pt(A[2], A[3]) + ' ' + pt(A[4], A[5]), Q.aw, p.furS, p.ol);
      s += C(A[4], A[5], Q.aw * 0.55, p.face, p.ol, 1.8);
    }
    // legs
    for (var j = 0; j < Q.legs.length; j++) {
      var L = Q.legs[j];
      s += tube('M' + pt(L[0], L[1]) + 'L' + pt(L[2], L[3]), Q.lw, j ? p.furS : p.fur, p.ol);
    }
    // body
    s += P(tuftD(50, Q.by, Q.brx, Q.bry, 13, 0.12, 0.4), p.fur, p.ol);
    if (!blurry) s += E(50, Q.by + Q.bry * 0.22, Q.brx * 0.55, Q.bry * 0.55, p.belly);
    // feet
    for (var k = 0; k < Q.feet.length; k++) {
      var F = Q.feet[k];
      s += bfFoot(F[0], F[1], F[2], F[3], F[4], p, blurry);
    }
    // ears
    s += C(hx - hr * 0.95, hy - hr * 0.05, hr * 0.24, p.fur, p.ol) + C(hx + hr * 0.95, hy - hr * 0.05, hr * 0.24, p.fur, p.ol);
    if (!blurry) s += C(hx - hr * 0.95, hy - hr * 0.05, hr * 0.11, p.face) + C(hx + hr * 0.95, hy - hr * 0.05, hr * 0.11, p.face);
    // head
    s += P(tuftD(hx, hy, hr, hr * 0.94, 14, 0.12, 3.6), p.fur, p.ol);
    if (Q.cowlick) s += P('M' + pt(hx - 3, hy - hr * 0.9) + 'Q' + pt(hx + 1, hy - hr * 1.45) + ' ' + pt(hx + 6, hy - hr * 1.2) + 'Q' + pt(hx + 2, hy - hr * 1.12) + ' ' + pt(hx + 2, hy - hr * 0.88) + 'Z', p.fur, p.ol, 1.8);
    // face mask: forehead dips into a soft brow
    var mx = hx, my = hy + hr * 0.2, mrx = hr * 0.7, mry = hr * 0.6;
    s += P('M' + pt(mx - mrx, my - mry * 0.1) + 'Q' + pt(mx - mrx * 0.6, my - mry * 1.05) + ' ' + pt(mx, my - mry * 0.62) +
      'Q' + pt(mx + mrx * 0.6, my - mry * 1.05) + ' ' + pt(mx + mrx, my - mry * 0.1) +
      'Q' + pt(mx + mrx * 1.02, my + mry) + ' ' + pt(mx, my + mry) + 'Q' + pt(mx - mrx * 1.02, my + mry) + ' ' + pt(mx - mrx, my - mry * 0.1) + 'Z', p.face, p.ol, 1.8);
    var er = Q.er, ey = hy + hr * 0.12;
    s += eyes({ x: hx, y: ey, gap: Q.eg, er: er, kind: 'big', pal: p, mood: o.mood, blink: o.blink, blurry: blurry });
    s += blush(hx, ey + er * 1.3, Q.eg, er, p, o.mood, blurry);
    if (!blurry) s += E(hx, hy + hr * 0.42, hr * 0.13, hr * 0.08, p.nose);
    s += mouth({ x: hx, y: hy + hr * 0.56, w: Q.mw, pal: p, mood: o.mood, blurry: blurry });
    if (Q.leaf && !blurry) {
      var lx = hx + hr * 0.45, ly = hy - hr * 0.82;
      s += P(leafD(lx, ly + 2, lx + 10, ly - 5, 4.6), p.leaf, p.ol, 1.6) + P('M' + pt(lx, ly + 2) + 'L' + pt(lx + 8, ly - 3.6), null, p.leafS, 1.1);
    }
    if (Q.shell) s += shellCap(hx - 1, hy - hr * 0.97, hr * 0.62, p.egg, p.ol, -14);
    return s;
  }
  var BF = {
    whisper: { hx: 50, hy: 68, hr: 15.5, by: 83, brx: 11, bry: 7, er: 3.6, eg: 5, mw: 3, shw: 17, shell: true, aw: 4, lw: 5,
      arms: [[42, 78, 38, 80, 36.5, 82.5], [58, 78, 62, 80, 63.5, 82.5]], legs: [[45, 86, 44, 88], [55, 86, 56, 88]],
      feet: [[42, 89.5, 7.5, 3.6, -4], [58, 89.5, 7.5, 3.6, 4]] },
    rumor: { hx: 50, hy: 51, hr: 17, by: 74, brx: 14.5, bry: 11.5, er: 3.9, eg: 5.4, mw: 3.4, shw: 20, aw: 5, lw: 6,
      arms: [[37, 68, 32, 74, 33, 80], [63, 68, 68, 74, 67, 80]], legs: [[43, 82, 42, 87], [57, 82, 58, 87]],
      feet: [[40, 89, 9.5, 4.3, -4], [60, 89, 9.5, 4.3, 4]] },
    sighting: { hx: 51, hy: 35, hr: 17, by: 59, brx: 14, bry: 15.5, er: 3.8, eg: 5.4, mw: 3.2, shw: 22, aw: 5, lw: 6, cowlick: true,
      arms: [[38, 50, 31, 64, 33, 78], [62, 50, 70, 64, 68, 78]], legs: [[44, 70, 41, 86], [56, 70, 59, 86]],
      feet: [[39, 88.5, 11.5, 4.8, -5], [61, 88.5, 11.5, 4.8, 5]] },
    legend: { hx: 50, hy: 31, hr: 21, by: 61, brx: 19, bry: 18.5, er: 4.4, eg: 6.6, mw: 4, shw: 26, aw: 6.5, lw: 8, leaf: true,
      arms: [[33, 52, 24, 60, 22, 70], [67, 52, 77, 56, 81, 64]], legs: [[42, 72, 33, 84], [58, 72, 67, 82]],
      feet: [[31, 87.5, 13, 5.6, -6], [69, 86.5, 12, 5.2, 14]] }
  };
  function bfEgg(o) {
    var p = PAL.bigfoot, cx = 50, cy = 64, rx = 20, ry = 26, s = '';
    s += P(eggD(cx, cy, rx, ry), p.egg, p.ol);
    s += P(crescentD(cx, cy + 4, 19.5), p.eggS);
    // moss cap
    s += P('M' + pt(cx - 15.5, cy - 15) + 'Q' + pt(cx - 10, cy - 30) + ' ' + pt(cx + 1, cy - 26) + 'Q' + pt(cx + 12, cy - 27) + ' ' + pt(cx + 16, cy - 14) +
      'Q' + pt(cx + 12, cy - 11) + ' ' + pt(cx + 9, cy - 14) + 'Q' + pt(cx + 5, cy - 9) + ' ' + pt(cx + 1, cy - 14) +
      'Q' + pt(cx - 4, cy - 10) + ' ' + pt(cx - 7, cy - 14) + 'Q' + pt(cx - 12, cy - 10) + ' ' + pt(cx - 15.5, cy - 15) + 'Z', p.moss, p.ol, 1.8);
    s += C(cx - 5, cy - 21, 1.6, p.mossS) + C(cx + 6, cy - 20, 1.3, p.mossS);
    // footprint
    var fp = 'M0,-6C6,-7 8,0 6,6C5,10 4,14 1,15C-3,16 -5,13 -4,9C-3,5 -7,0 -6,-3C-5.5,-5.5 -3,-6 0,-6Z';
    var toes = [[-4.8, -9.6, 2.3], [-0.8, -11.6, 2], [3, -11.2, 1.8], [6, -9.4, 1.6], [8, -6.6, 1.4]];
    var fps = P(fp, p.print);
    for (var i = 0; i < toes.length; i++) fps += C(toes[i][0], toes[i][1], toes[i][2], p.print);
    s += G(fps, ' transform="translate(' + (cx - 0.5) + ' ' + (cy + 3) + ') rotate(8) scale(1.12)"');
    s += C(cx - 12, cy + 6, 1.4, p.eggS) + C(cx + 13, cy + 2, 1.2, p.eggS) + C(cx + 10, cy + 16, 1.5, p.eggS);
    if (o.mood === 'happy') s += crack(cx + 3, cy + 26, 8, 16, p.ol);
    return eggWrap(s, o, 90, 17);
  }

  // =====================================================================
  // NESSIE
  // =====================================================================
  function water(cx, cy, rx, ry, p, simple) {
    var s = E(cx, cy, rx, ry, p.water, p.ol);
    if (simple) return s;
    s += P('M' + pt(cx - rx * 0.62, cy - ry * 0.05) + 'Q' + pt(cx - rx * 0.45, cy - ry * 0.45) + ' ' + pt(cx - rx * 0.28, cy - ry * 0.05), null, p.waterHi, 1.4);
    s += P('M' + pt(cx + rx * 0.2, cy + ry * 0.25) + 'Q' + pt(cx + rx * 0.38, cy - ry * 0.2) + ' ' + pt(cx + rx * 0.56, cy + ry * 0.25), null, p.waterHi, 1.4);
    s += P('M' + pt(cx - rx * 0.1, cy + ry * 0.45) + 'L' + pt(cx + rx * 0.05, cy + ry * 0.45), null, p.waterHi, 1.4);
    return s;
  }
  function nsDraw(Q, o, p, id, blurry) {
    var s = '', hx = Q.hx, hy = Q.hy, hr = Q.hr;
    // back humps and tail
    for (var i = 0; i < Q.humps.length; i++) {
      var H = Q.humps[i];
      s += E(H[0], H[1], H[2], H[3], p.body, p.ol);
      if (!blurry) s += C(H[0] - H[2] * 0.2, H[1] - H[3] * 0.45, Math.max(1.2, H[2] * 0.16), p.spot);
    }
    if (Q.tail) {
      var T = Q.tail;
      s += P('M' + pt(T[0] - 4, T[1] + 4) + 'Q' + pt(T[0] + 1, T[1] - 6) + ' ' + pt(T[0] + 6, T[1] - 8) + 'Q' + pt(T[0] + 4, T[1] - 1) + ' ' + pt(T[0] + 4, T[1] + 4) + 'Z', p.body, p.ol);
    }
    // main body
    s += E(Q.bx, Q.by, Q.brx, Q.bry, p.body, p.ol);
    if (!blurry) {
      s += C(Q.bx + Q.brx * 0.35, Q.by - Q.bry * 0.45, Q.brx * 0.12, p.spot) + C(Q.bx + Q.brx * 0.6, Q.by - Q.bry * 0.1, Q.brx * 0.08, p.spot);
    }
    // flippers
    for (var f = 0; f < Q.flip.length; f++) {
      var F = Q.flip[f];
      s += E(F[0], F[1], F[2], F[3], p.bodyS, p.ol, 1.8, rot(F[4], F[0], F[1]));
    }
    // neck
    s += tube(Q.neck, Q.nw, p.body, p.ol);
    if (!blurry) s += P(Q.belly, null, p.belly, Q.nw * 0.42);
    // head nubs
    if (!blurry) {
      s += C(hx - hr * 0.42, hy - hr * 0.98, hr * 0.2, p.bodyS, p.ol, 1.6) + C(hx + hr * 0.42, hy - hr * 0.98, hr * 0.2, p.bodyS, p.ol, 1.6);
    }
    // head
    s += C(hx, hy, hr, p.body, p.ol);
    if (!blurry) {
      s += E(hx, hy + hr * 0.5, hr * 0.62, hr * 0.36, p.belly);
      s += C(hx + hr * 0.62, hy - hr * 0.5, hr * 0.1, p.spot) + C(hx - hr * 0.7, hy - hr * 0.35, hr * 0.08, p.spot);
    }
    var er = Q.er, ey = hy - hr * 0.05;
    s += eyes({ x: hx, y: ey, gap: Q.eg, er: er, kind: 'big', pal: p, mood: o.mood, blink: o.blink, blurry: blurry });
    s += blush(hx, ey + er * 1.25, Q.eg, er, p, o.mood, blurry);
    s += mouth({ x: hx, y: hy + hr * 0.5, w: Q.mw, pal: p, mood: o.mood, blurry: blurry });
    if (Q.shell) s += shellCap(hx + 1, hy - hr, hr * 0.6, p.egg, p.ol, 10);
    // water in front
    s += water(50, Q.wy, Q.wrx, Q.wry, p, blurry);
    // crown of droplets
    if (Q.crown && !blurry) {
      var bob = Math.sin(o.t * 3) * 0.8;
      s += P(dropD(hx - 11, hy - hr - 2 + bob, 2.2), p.water, p.ol, 1.4) + P(dropD(hx, hy - hr - 5.2 - bob, 2.5), p.water, p.ol, 1.4) +
        P(dropD(hx + 11, hy - hr - 2 + bob, 2.2), p.water, p.ol, 1.4);
      s += C(hx - 0.8, hy - hr - 5.8 - bob, 0.7, p.hi);
    }
    return s;
  }
  var NS = {
    whisper: { hx: 50, hy: 68, hr: 15, bx: 50, by: 85, brx: 12, bry: 7, humps: [[66, 87, 5, 4]], flip: [[38, 86, 4, 2.2, -25]],
      neck: 'M50,84 L50,77', belly: 'M51.6,84 L51.6,78', nw: 10, er: 4.6, eg: 6, mw: 3, shell: true, wy: 89.5, wrx: 27, wry: 4 },
    rumor: { hx: 48, hy: 48, hr: 15, bx: 48, by: 82, brx: 15, bry: 9.5, humps: [[69, 85, 6, 5]], tail: [80, 84], flip: [[35, 84, 4.6, 2.4, -25], [58, 86, 4, 2.2, 20]],
      neck: 'M46,80 C43,70 48,66 48,58', belly: 'M48.6,79 C46,70 50.5,66 50.5,59', nw: 10, er: 4.7, eg: 6.2, mw: 3.2, wy: 88.5, wrx: 33, wry: 4.5 },
    sighting: { hx: 46, hy: 27, hr: 12.5, bx: 46, by: 82, brx: 17, bry: 10.5, humps: [[69, 85, 6.5, 5.5], [82, 87, 4.5, 3.8]], flip: [[31, 85, 5, 2.4, -25], [60, 86, 4.4, 2.2, 22]],
      neck: 'M42,79 C32,62 56,52 46,33', belly: 'M44.6,78 C35.5,62 58.2,51 48.6,34', nw: 8, er: 4, eg: 5.2, mw: 2.8, wy: 88.5, wrx: 38, wry: 4.5 },
    legend: { hx: 48, hy: 27, hr: 14.5, bx: 44, by: 82, brx: 21, bry: 13, humps: [[72, 84, 8, 7]], tail: [86, 84], flip: [[27, 84, 5.5, 2.7, -25], [60, 86, 5, 2.5, 22]],
      neck: 'M38,78 C33,62 49,54 48,36', belly: 'M41,77 C36.5,62 52,54.5 51,37', nw: 11, er: 4.8, eg: 6.4, mw: 3.4, crown: true, wy: 88.5, wrx: 42, wry: 5 }
  };
  function nsEgg(o) {
    var p = PAL.nessie, cx = 50, cy = 63, rx = 18, ry = 23, s = '';
    s += P(eggD(cx, cy, rx, ry), p.egg, p.ol);
    s += P(crescentD(cx, cy + 3, 17.5), p.body, null, null, ' opacity="0.55"');
    var sp = [[-8, -10, 2.2, 0], [5, -14, 1.6, 0], [9, -3, 2.4, 0], [-10, 3, 1.5, 0], [2, 4, 1.8, 0], [-3, -4, 1.3, 1], [7, 8, 1.3, 1], [-6, -16, 1.1, 1]];
    for (var i = 0; i < sp.length; i++) s += C(cx + sp[i][0], cy + sp[i][1], sp[i][2], sp[i][3] ? p.speckL : p.speck);
    s += P('M' + pt(cx - 10, cy - 15) + 'Q' + pt(cx - 12, cy - 9) + ' ' + pt(cx - 12.5, cy - 4), null, p.hi, 1.6, ' opacity="0.7"');
    if (o.mood === 'happy') s += crack(cx, cy, rx, ry, p.ol);
    var inner = eggWrap(s, o, 86, 0);
    return inner + water(50, 88.5, 30, 4.5, p, false);
  }

  // =====================================================================
  // FRESNO NIGHTCRAWLER
  // =====================================================================
  function ncDraw(Q, o, p, id, blurry) {
    var s = '', hx = Q.hx, hy = Q.hy, hr = Q.hr;
    if (!blurry) s += shadow(Q.shw);
    // back leg (shade) then front leg
    var legs = Q.legs;
    for (var i = 0; i < legs.length; i++) {
      var L = legs[i], col = i === 0 ? p.shade : p.body;
      s += E(L.fx, L.fy, Q.fw, Q.fw * 0.5, p.foot, p.ol, 1.6, L.fa ? rot(L.fa, L.fx, L.fy) : '');
      s += P(pantD(L.p, Q.pw * 0.5, Q.pw * 0.8), col, p.ol);
    }
    // head
    s += C(hx, hy, hr, p.body);
    if (!blurry) s += P(crescentD(hx, hy, hr), p.shade);
    s += C(hx, hy, hr, null, p.ol);
    if (Q.bow && !blurry) {
      var by = hy + hr * 0.98;
      s += P('M' + pt(hx, by) + 'L' + pt(hx - 4.5, by - 2.6) + 'L' + pt(hx - 4.5, by + 2.6) + 'Z' + 'M' + pt(hx, by) + 'L' + pt(hx + 4.5, by - 2.6) + 'L' + pt(hx + 4.5, by + 2.6) + 'Z', p.bow, p.ol, 1.4);
      s += C(hx, by, 1.3, p.bow, p.ol, 1.2);
    }
    var er = Q.er, ey = hy - hr * 0.08;
    s += eyes({ x: hx, y: ey, gap: Q.eg, er: er, kind: 'dot', pal: p, mood: o.mood, blink: o.blink, blurry: blurry });
    s += blush(hx, ey + er * 1.9, Q.eg, er, p, o.mood, blurry);
    s += mouth({ x: hx, y: hy + hr * 0.38, w: Q.mw, pal: p, mood: o.mood, blurry: blurry });
    if (Q.shell) s += shellCap(hx - 1, hy - hr * 0.97, hr * 0.62, p.body, p.ol, -12);
    return s;
  }
  function leg(x0, y0, cx, cy, x1, y1, fx, fy, fa) {
    return { p: [x0, y0, cx, cy, x1, y1], fx: fx, fy: fy, fa: fa || 0 };
  }
  // loose pant leg: a quadratic center line, widening from w0 (hip) to w1 (hem), flat hem
  function pantD(q, w0, w1) {
    var left = [], right = [], N = 7;
    for (var k = 0; k <= N; k++) {
      var t = k / N, u = 1 - t;
      var x = u * u * q[0] + 2 * u * t * q[2] + t * t * q[4], y = u * u * q[1] + 2 * u * t * q[3] + t * t * q[5];
      var dx = 2 * u * (q[2] - q[0]) + 2 * t * (q[4] - q[2]), dy = 2 * u * (q[3] - q[1]) + 2 * t * (q[5] - q[3]);
      var len = Math.sqrt(dx * dx + dy * dy) || 1, w = w0 + (w1 - w0) * Math.pow(t, 1.6);
      var nx = -dy / len * w, ny = dx / len * w;
      if (k === N) { nx = (nx < 0 ? -1 : 1) * w * 1.15; ny = 0; } // level hem
      left.push(pt(x + nx, y + ny)); right.unshift(pt(x - nx, y - ny));
    }
    return 'M' + left.join('L') + 'L' + right.join('L') + 'Z';
  }
  var NC = {
    whisper: { hx: 50, hy: 63, hr: 14, wy: 76, ww: 8, wh: 3.5, pw: 6, fw: 3.4, er: 2.6, eg: 5, mw: 2.6, shw: 12, shell: true,
      legs: [leg(53, 73, 54, 80, 54.5, 88.5, 55.5, 90.6, 0), leg(47, 73, 46, 80, 45.5, 88.5, 44.5, 90.6, 0)] },
    rumor: { hx: 50, hy: 44, hr: 12, wy: 56, ww: 7.5, wh: 3.4, pw: 5.6, fw: 3.2, er: 2.3, eg: 4.4, mw: 2.4, shw: 12,
      legs: [leg(53, 53, 55, 70, 55.5, 88.5, 56.5, 90.6, 0), leg(47, 53, 45, 70, 44.5, 88.5, 43.5, 90.6, 0)] },
    sighting: { hx: 51, hy: 27, hr: 10.5, wy: 37.5, ww: 6.8, wh: 3, pw: 5, fw: 3, er: 2.1, eg: 3.9, mw: 2.2, shw: 12,
      legs: [leg(53, 35, 49.5, 64, 57, 88.5, 58.5, 90.6, 8), leg(47, 35, 50.5, 64, 43, 88.5, 41.5, 90.6, -8)] },
    legend: { hx: 50, hy: 17, hr: 10.5, wy: 28, ww: 6.8, wh: 3, pw: 5, fw: 3.2, er: 2.2, eg: 4, mw: 2.3, shw: 20, bow: true,
      legs: [leg(53, 25, 59, 56, 69, 85.5, 71.5, 88.2, 28), leg(47, 25, 43, 56, 31, 88.5, 29, 90.6, -6)] }
  };
  function ncEgg(o) {
    var p = PAL.nightcrawler, cx = 50, cy = 62, rx = 13, ry = 17, s = '';
    var bounce = o.mood === 'happy' ? Math.abs(Math.sin(o.t * 5)) * 1.2 : 0;
    s += tube('M' + pt(46, 75) + 'L' + pt(45, 89), 2.2, p.body, p.ol) + tube('M' + pt(54, 75) + 'L' + pt(55, 89 - bounce), 2.2, p.body, p.ol);
    s += E(43.5, 90.4, 2.8, 1.4, p.foot, p.ol, 1.4) + E(56.5, 90.4 - bounce, 2.8, 1.4, p.foot, p.ol, 1.4);
    var e = P(eggD(cx, cy, rx, ry), p.body, p.ol) + P(crescentD(cx, cy + 2, 12.5), p.shade);
    e += C(cx - 5, cy - 6, 1.4, p.shade) + C(cx + 4, cy - 9, 1.1, p.shade) + C(cx + 6, cy + 2, 1.2, p.shade);
    if (o.mood === 'happy') e += crack(cx, cy, rx, ry, p.ol);
    return eggWrap(s + e, o, 90, 12);
  }

  // =====================================================================
  // assembly
  // =====================================================================
  function build(code, species, draw, Q, eggFn) {
    var p = PAL[species], bp = BPAL[species], out = {};
    out.egg = function (opts) { return eggFn(moodOf(opts)); };
    ['whisper', 'rumor', 'sighting', 'legend'].forEach(function (form) {
      out[form] = function (opts) { return draw(Q[form], moodOf(opts), p, code + '-' + form, false); };
    });
    out.blurry = function (opts) {
      var o = moodOf(opts);
      // the neglected adult: always a little mopey, but still blinks and sleeps
      o.mood = o.mood === 'sleep' ? 'sleep' : 'sad';
      o.t = 0;
      return (Q.legend.shw ? shadow(Q.legend.shw) : '') + blurryWrap(code + '-blurry', draw(Q.legend, o, bp, code + '-blurry', true));
    };
    return out;
  }

  window.CRYPTID_SPRITES = {
    mothman: build('mm', 'mothman', mmDraw, MM, mmEgg),
    bigfoot: build('bf', 'bigfoot', bfDraw, BF, bfEgg),
    nessie: build('ns', 'nessie', nsDraw, NS, nsEgg),
    nightcrawler: build('nc', 'nightcrawler', ncDraw, NC, ncEgg)
  };
})();
