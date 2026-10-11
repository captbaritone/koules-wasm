/*
 * touchstick.js -- the touchscreen controller.
 *
 * A floating analog stick, the way most mobile action games steer:
 *
 *  - It appears wherever your thumb lands, so there is nothing to aim
 *    for. In portrait the play field moves to the top and the space
 *    below is the stick's home; a faint ghost of it rests there.
 *  - It is analog. The rocket points where the stick points and thrusts
 *    in proportion to how far it is pushed, through the game's own
 *    analog joystick mode (C_JOYSTICK1) -- so a light touch is a nudge.
 *  - It follows your thumb. Push past the rim and the stick slides
 *    after you, so you never run out of travel or have to re-centre.
 *  - It has a small deadzone at the centre and reaches full thrust
 *    before the rim, so a resting thumb drifts nowhere and full power
 *    needs no stretch.
 *
 * During play the whole screen steers; in the menus taps go through to
 * the game untouched. Touches the stick takes are kept from SDL, so
 * they never also become clicks.
 *
 * The game side is koules_touchstick() and koules_playing() in
 * sdl/input.c. Mouse and keyboard are untouched: this only listens to
 * touch.
 */
(function () {
  'use strict';

  var R = 60;            /* stick radius, CSS px */
  var DEAD = 0.14;       /* no thrust inside this fraction of R */
  var FULL = 0.85;       /* full thrust from here out */
  var DIRS = 64;         /* headings; the wire carries 32 anyway */
  var LEVELS = 8;        /* thrust steps */
  var JOYFULL = 1000;    /* full deflection, as koules.h has it */

  /* ?touch forces it on, for touchscreens that don't say so. */
  var touchscreen = navigator.maxTouchPoints > 0 || 'ontouchstart' in window ||
      /[?&]touch\b/.test(location.search);
  if (!touchscreen) return;
  var root = document.documentElement;
  root.classList.add('ks-touch');

  var style = document.createElement('style');
  style.textContent = [
    'html.ks-touch, html.ks-touch body { touch-action: none; overscroll-behavior: none; -webkit-touch-callout: none; }',
    /* Portrait: play field on top, the stick's home below. */
    '#ks-pad { display: none; }',
    '@media (orientation: portrait) {',
    '  html.ks-touch body { flex-direction: column; justify-content: flex-start; }',
    '  html.ks-touch #loader { margin: auto; }',
    '  html.ks-touch canvas.running ~ #ks-pad { display: block; flex: 1; align-self: stretch; }',
    '}',
    '#ks-layer { position: fixed; inset: 0; pointer-events: none; z-index: 2; overflow: hidden; }',
    '#ks-stick { position: absolute; left: 0; top: 0; width: ' + 2 * R + 'px; height: ' + 2 * R + 'px;',
    '  margin: ' + -R + 'px 0 0 ' + -R + 'px; opacity: 0; will-change: transform, opacity;',
    '  transition: opacity .25s ease, transform .35s cubic-bezier(.2,.8,.2,1); }',
    '#ks-stick.ghost { opacity: .22; }',
    '#ks-stick.live { opacity: 1; transition: opacity .08s ease; }',
    '.ks-base { position: absolute; inset: 0; border-radius: 50%;',
    '  background: radial-gradient(circle, rgba(255,255,255,.05) 55%, rgba(255,255,255,.13));',
    '  box-shadow: inset 0 0 0 1.5px rgba(255,255,255,.3); }',
    '.ks-arc { position: absolute; inset: -4px; border-radius: 50%; opacity: 0;',
    '  background: conic-gradient(from -40deg, rgba(255,214,90,0), rgba(255,214,90,.95) 40deg, rgba(255,214,90,0) 80deg, transparent 0);',
    '  -webkit-mask: radial-gradient(farthest-side, transparent calc(100% - 5px), #000 calc(100% - 4px));',
    '          mask: radial-gradient(farthest-side, transparent calc(100% - 5px), #000 calc(100% - 4px)); }',
    '.ks-knob { position: absolute; left: 50%; top: 50%; width: 50px; height: 50px; margin: -25px 0 0 -25px;',
    '  border-radius: 50%; background: radial-gradient(circle at 38% 32%, #fff, #d4dae6 55%, #8c97ab);',
    '  box-shadow: 0 3px 12px rgba(0,0,0,.55);',
    '  transition: transform .3s cubic-bezier(.3,1.5,.5,1); }',
    '#ks-stick.live .ks-knob { transition: none; }',
    '.ks-hint { position: absolute; left: 50%; top: 100%; margin-top: 14px; transform: translateX(-50%);',
    '  white-space: nowrap; font: 12px monospace; letter-spacing: .08em; color: #fff; opacity: 0;',
    '  transition: opacity .4s ease; }',
    '#ks-stick.ghost.fresh .ks-hint { opacity: 1; }',
  ].join('\n');
  document.head.appendChild(style);

  var canvas = document.getElementById('canvas');
  var pad = document.createElement('div');
  pad.id = 'ks-pad';
  canvas.parentNode.insertBefore(pad, canvas.nextSibling);

  var layer = document.createElement('div');
  layer.id = 'ks-layer';
  layer.innerHTML =
    '<div id="ks-stick"><div class="ks-base"></div><div class="ks-arc"></div>' +
    '<div class="ks-knob"></div><div class="ks-hint">TOUCH &amp; DRAG TO FLY</div></div>';
  document.body.appendChild(layer);
  var stick = document.getElementById('ks-stick');
  var arc = stick.querySelector('.ks-arc');
  var knob = stick.querySelector('.ks-knob');

  /* Until someone has used it once in this browser, the ghost says how. */
  var SEEN = 'koules.touchstick.seen';
  try { if (!localStorage.getItem(SEEN)) stick.classList.add('fresh'); }
  catch (e) { stick.classList.add('fresh'); }

  function game(name) { return window.Module && Module['_koules_' + name]; }
  function playing() { var f = game('playing'); return !!(f && f()); }
  function buzz(ms) { if (navigator.vibrate) navigator.vibrate(ms); }

  var finger = null;          /* identifier of the steering touch */
  var ox = 0, oy = 0;         /* stick centre */
  var sent = '';              /* last deflection handed to the game */
  var wasFull = false;

  function send(active, jx, jy) {
    var key = active + ',' + jx + ',' + jy;
    if (key === sent) return;
    sent = key;
    var f = game('touchstick');
    if (f) f(active, jx, jy);
  }

  function moveStick(x, y) {
    stick.style.transform = 'translate(' + x + 'px,' + y + 'px)';
  }

  /* Where the ghost rests: the middle of the pad, a little low, where a
   * thumb falls. Nowhere in landscape -- there the stick only appears
   * under a thumb. */
  function rest() {
    var r = pad.getBoundingClientRect();
    if (r.height < 2 * R + 40 || !playing()) return null;
    return { x: r.left + r.width / 2, y: r.top + r.height * 0.52 };
  }

  function settle() {
    var p = rest();
    stick.classList.remove('live');
    stick.classList.toggle('ghost', !!p);
    if (p) moveStick(p.x, p.y);
    knob.style.transform = '';
    arc.style.opacity = 0;
  }

  function steer(x, y) {
    var vx = x - ox, vy = y - oy, d = Math.hypot(vx, vy);
    if (d > R) {
      /* Past the rim: drag the stick along behind the thumb. */
      ox += vx * (d - R) / d;
      oy += vy * (d - R) / d;
      vx = x - ox; vy = y - oy; d = R;
      moveStick(ox, oy);
    }
    knob.style.transform = 'translate(' + vx + 'px,' + vy + 'px)';

    var n = d / R;
    if (n < DEAD) {
      arc.style.opacity = 0;
      wasFull = false;
      send(1, 0, 0);
      return;
    }
    var m = Math.min(1, (n - DEAD) / (FULL - DEAD));
    m = Math.ceil(m * LEVELS) / LEVELS;
    var step = 2 * Math.PI / DIRS;
    var a = Math.round(Math.atan2(vy, vx) / step) * step;
    /* The game's sign convention: positive is left and up. */
    send(1, Math.round(-Math.cos(a) * m * JOYFULL),
            Math.round(-Math.sin(a) * m * JOYFULL));

    arc.style.opacity = 0.25 + 0.75 * m;
    arc.style.transform = 'rotate(' + (a * 180 / Math.PI + 90) + 'deg)';
    if (m === 1 && !wasFull) buzz(4);
    wasFull = m === 1;
  }

  function begin(t) {
    finger = t.identifier;
    ox = t.clientX; oy = t.clientY;
    stick.classList.add('live');
    stick.classList.remove('ghost', 'fresh');
    moveStick(ox, oy);
    wasFull = false;
    steer(ox, oy);
    buzz(8);
    try { localStorage.setItem(SEEN, '1'); } catch (e) { }
  }

  function end() {
    finger = null;
    send(0, 0, 0);
    settle();
  }

  function mine(e) {
    for (var i = 0; i < e.changedTouches.length; i++)
      if (e.changedTouches[i].identifier === finger) return e.changedTouches[i];
    return null;
  }

  /* Capture phase, on window: this runs before SDL's listeners on the
   * canvas, so a touch the stick takes can be kept from them. */
  window.addEventListener('touchstart', function (e) {
    if (finger !== null || !playing()) return;
    var t = e.changedTouches[0];
    if (t.target.closest && t.target.closest('a')) return;
    e.preventDefault();
    e.stopPropagation();
    begin(t);
  }, { capture: true, passive: false });

  window.addEventListener('touchmove', function (e) {
    var t = mine(e);
    if (!t) return;
    e.preventDefault();
    e.stopPropagation();
    steer(t.clientX, t.clientY);
  }, { capture: true, passive: false });

  function lift(e) {
    if (!mine(e)) return;
    e.preventDefault();
    e.stopPropagation();
    end();
  }
  window.addEventListener('touchend', lift, { capture: true, passive: false });
  window.addEventListener('touchcancel', lift, { capture: true, passive: false });

  /* Follow the game in and out of play: show the ghost when a game
   * starts, hide it in the menus, and let go if play stops under a
   * thumb. Cheap enough to check every frame. */
  var wasPlaying = false;
  (function watch() {
    var now = playing();
    if (now !== wasPlaying) {
      wasPlaying = now;
      if (!now && finger !== null) end();
      else if (finger === null) settle();
    }
    requestAnimationFrame(watch);
  })();
  window.addEventListener('resize', function () { if (finger === null) settle(); });
})();
