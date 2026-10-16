// TinyBuddy face lab — front end. Frames come from build/tbsim (the firmware's own renderer)
// as 1024-byte page-layout buffers; this file only decodes and paints them.
"use strict";

const $ = (s, el = document) => el.querySelector(s);
const $$ = (s, el = document) => [...el.querySelectorAll(s)];
const W = 128, H = 64;

async function api(path, opts) {
  const r = await fetch(path, opts);
  const j = await r.json();
  if (!r.ok) throw new Error(j.error || `HTTP ${r.status}`);
  return j;
}
const post = (path, body) => api(path, { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) });

// ---------------------------------------------------------------- frames & painting
function decode(b64) {
  const s = atob(b64);
  const u = new Uint8Array(s.length);
  for (let i = 0; i < s.length; i++) u[i] = s.charCodeAt(i);
  return u;
}
const isLit = (buf, x, y) => (buf[(y >> 3) * W + x] >> (y & 7)) & 1;

const TINT = {
  white: () => [226, 240, 255],
  blue: () => [118, 196, 255],
  yellow: (y) => (y < 16 ? [255, 226, 110] : [118, 196, 255]),  // classic two-colour module
};
let tintName = localStore("tint") || "white";

function localStore(k, v) {
  try {
    if (v === undefined) return localStorage.getItem("tb." + k);
    localStorage.setItem("tb." + k, v);
  } catch (_) { return null; }
}

const small = document.createElement("canvas");
small.width = W; small.height = H;

function paint(cv, buf, { scale = 4, glow = true, tint = tintName, crispOnly = false } = {}) {
  const gap = scale >= 3 ? 1 : 0;
  if (cv.width !== W * scale) { cv.width = W * scale; cv.height = H * scale; }
  const ctx = cv.getContext("2d");
  ctx.imageSmoothingEnabled = false;
  ctx.fillStyle = crispOnly ? "#000" : "#05070a";
  ctx.fillRect(0, 0, cv.width, cv.height);
  const tf = TINT[tint] || TINT.white;
  if (glow) {
    const sctx = small.getContext("2d");
    const img = sctx.createImageData(W, H);
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
      if (!isLit(buf, x, y)) continue;
      const c = tf(y), o = (y * W + x) * 4;
      img.data[o] = c[0]; img.data[o + 1] = c[1]; img.data[o + 2] = c[2]; img.data[o + 3] = 255;
    }
    sctx.putImageData(img, 0, 0);
    ctx.imageSmoothingEnabled = true;
    ctx.globalAlpha = 0.28;
    ctx.drawImage(small, -scale, -scale, cv.width + 2 * scale, cv.height + 2 * scale);
    ctx.globalAlpha = 0.18;
    ctx.drawImage(small, -scale * 2.5, -scale * 2.5, cv.width + 5 * scale, cv.height + 5 * scale);
    ctx.globalAlpha = 1;
    ctx.imageSmoothingEnabled = false;
  }
  let lastRow = -1;
  for (let y = 0; y < H; y++) {
    const c = tf(y);
    if (y === 0 || (tint === "yellow" && y === 16) || lastRow < 0) { ctx.fillStyle = crispOnly ? "#fff" : `rgb(${c})`; lastRow = y; }
    for (let x = 0; x < W; x++) if (isLit(buf, x, y)) ctx.fillRect(x * scale, y * scale, scale - gap, scale - gap);
  }
}

function paintNative(cv, buf, scale = 1) {
  cv.width = W * scale; cv.height = H * scale;
  const ctx = cv.getContext("2d");
  const img = ctx.createImageData(W * scale, H * scale);
  for (let y = 0; y < H * scale; y++) for (let x = 0; x < W * scale; x++) {
    const v = isLit(buf, (x / scale) | 0, (y / scale) | 0) ? 255 : 0, o = (y * W * scale + x) * 4;
    img.data[o] = img.data[o + 1] = img.data[o + 2] = v; img.data[o + 3] = 255;
  }
  ctx.putImageData(img, 0, 0);
}

function download(cv, name) {
  const a = document.createElement("a");
  a.href = cv.toDataURL("image/png");
  a.download = name;
  a.click();
}

// ---------------------------------------------------------------- app state
const S = {
  info: null,
  reviews: { current: {}, history: [] },
  approved: null,
  expr: "rest",
  poseCache: new Map(),
  anim: { mode: "anim", face: "F", id: 80, mirror: false, speed: 1, loop: true, playing: false, t: 0, data: null, sync: null, frames: null },
  tune: { face: "F", sets: {}, params: [], styles: null },
};

async function pose(face, expr, sets = []) {
  const key = face + "|" + expr + "|" + sets.join("&");
  if (S.poseCache.has(key)) return S.poseCache.get(key);
  const q = new URLSearchParams({ face, expr });
  sets.forEach((s) => q.append("set", s));
  const p = api("/api/pose?" + q).then((j) => decode(j.frame));
  S.poseCache.set(key, p);
  return p;
}

// ---------------------------------------------------------------- tabs
$$(".tabs button").forEach((b) => b.addEventListener("click", () => showTab(b.dataset.tab)));
function showTab(name) {
  $$(".tabs button").forEach((b) => b.classList.toggle("on", b.dataset.tab === name));
  $$(".tab").forEach((t) => t.classList.toggle("on", t.id === "tab-" + name));
  localStore("tabname", name);
  if (name === "animate" && !S.anim.data && !S.anim.sync) loadAnim();
  if (name === "tune" && !S.tune.params.length) initTune();
  if (name === "history") loadRounds();
}

// ---------------------------------------------------------------- FACES
const EXPR_LABEL = (e) => e.replace("_", " ");

function buildFaces() {
  const chips = $("#exprChips");
  ["rest", ...S.info.expressions].forEach((e) => {
    const b = document.createElement("button");
    b.textContent = e === "rest" ? "resting" : EXPR_LABEL(e);
    b.dataset.expr = e;
    b.className = e === S.expr ? "on" : "";
    b.onclick = () => setExpr(e);
    chips.appendChild(b);
  });
  const grid = $("#faceGrid");
  S.info.faces.forEach((f) => {
    const card = document.createElement("article");
    card.className = "card";
    card.dataset.face = f.id;
    card.innerHTML = `
      <div class="bezel"><canvas width="512" height="256" aria-label="Face ${f.id} preview"></canvas></div>
      <div class="card-head"><div><h3>${f.id} · ${f.name}</h3><div class="motto">${f.motto}</div></div><span class="badge">not reviewed</span></div>
      <div class="thumbs"><canvas class="t2"></canvas><canvas class="t1"></canvas><small>2× · 1× true pixels</small></div>
      <div class="verdicts">
        <button data-v="approved">👍 Love it</button><button data-v="maybe">🤔 Maybe</button><button data-v="rejected">👎 Not this</button>
        <button data-v="canonical" class="primary" title="Freeze as the canonical TinyBuddy face">★ Make canonical</button>
      </div>
      <textarea rows="2" placeholder="Notes for Claude: what to keep, what to change…"></textarea>`;
    grid.appendChild(card);
    $$(".verdicts button", card).forEach((b) => (b.onclick = () => verdict(f.id, b.dataset.v, card)));
  });
  refreshVerdicts();
  renderFaces();
}

function setExpr(e) {
  S.expr = e;
  $$("#exprChips button").forEach((b) => b.classList.toggle("on", b.dataset.expr === e));
  renderFaces();
}

async function renderFaces() {
  await Promise.all(S.info.faces.map(async (f) => {
    const buf = await pose(f.id, S.expr);
    const card = $(`.card[data-face="${f.id}"]`);
    paint($(".bezel canvas", card), buf, { scale: 4 });
    paintNative($(".t2", card), buf, 2);
    paintNative($(".t1", card), buf, 1);
  }));
}

function refreshVerdicts() {
  S.info.faces.forEach((f) => {
    const card = $(`.card[data-face="${f.id}"]`);
    const v = S.reviews.current[f.id];
    card.classList.remove("v-approved", "v-maybe", "v-rejected");
    const badge = $(".badge", card);
    badge.className = "badge";
    $$(".verdicts button", card).forEach((b) => b.classList.toggle("on", !!v && b.dataset.v === v.verdict));
    if (S.approved && S.approved.face === f.id) {
      badge.textContent = "★ canonical";
      badge.classList.add("canonical");
      card.classList.add("v-approved");
    } else if (v) {
      badge.textContent = v.verdict;
      badge.classList.add(v.verdict);
      card.classList.add("v-" + v.verdict);
    } else badge.textContent = "not reviewed";
    const ta = $("textarea", card);
    if (v && !ta.value) ta.value = v.note || "";
  });
  const box = $("#approvedBox");
  if (S.approved) {
    box.hidden = false;
    box.innerHTML = `<strong>Canonical face: ${S.approved.face} · ${S.approved.name}</strong> — saved to <code>reference/approved_face.json</code> on ${S.approved.approved_at}. Tell Claude in chat to continue with the full animation catalog.`;
  } else box.hidden = true;
}

async function verdict(face, v, card) {
  const note = $("textarea", card).value;
  if (v === "canonical") return confirmCanonical(face, note);
  const cur = S.reviews.current[face];
  const next = cur && cur.verdict === v ? "clear" : v;
  S.reviews = await post("/api/review", { face, verdict: next, note });
  refreshVerdicts();
}

function confirmCanonical(face, note, sets = []) {
  const f = S.info.faces.find((x) => x.id === face);
  const dlg = $("#confirm");
  $("#confirmTitle").textContent = `Make ${face} · ${f.name} the canonical TinyBuddy face?`;
  $("#confirmText").textContent = sets.length
    ? `With your tuning tweaks: ${sets.join("  ")}. This writes reference/approved_face.json and reference renders.`
    : "This writes reference/approved_face.json and reference renders, which every later animation must follow.";
  $("#confirmNote").value = note || "";
  dlg.returnValue = "";
  dlg.onclose = async () => {
    if (dlg.returnValue !== "ok") return;
    await post("/api/approve", { face, note: $("#confirmNote").value, set: sets });
    const r = await api("/api/reviews");
    S.reviews = r; S.approved = r.approved;
    refreshVerdicts();
  };
  dlg.showModal();
}

let cycleTimer = null;
$("#cycle").addEventListener("change", (e) => {
  clearInterval(cycleTimer);
  if (!e.target.checked) return;
  const list = ["rest", ...S.info.expressions];
  cycleTimer = setInterval(() => setExpr(list[(list.indexOf(S.expr) + 1) % list.length]), 1500);
});
$("#tint").value = tintName;
$("#tint").addEventListener("change", (e) => {
  tintName = e.target.value; localStore("tint", tintName);
  renderFaces(); drawAnimFrame(); renderTune();
});

// ---------------------------------------------------------------- ANIMATE
const A = S.anim;

function buildAnimate() {
  const chips = $("#animFaceChips");
  S.info.faces.forEach((f) => {
    const b = document.createElement("button");
    b.textContent = `${f.id} · ${f.name}`;
    b.dataset.face = f.id;
    b.className = f.id === A.face ? "on" : "";
    b.onclick = () => { A.face = f.id; $$("#animFaceChips button").forEach((x) => x.classList.toggle("on", x === b)); loadAnim(); };
    chips.appendChild(b);
  });
  const sel = $("#animSel");
  S.info.animations.forEach((a) => {
    const o = document.createElement("option");
    o.value = a.id; o.textContent = `#${a.id} · ${a.name}`;
    sel.appendChild(o);
  });
  sel.onchange = () => { A.id = +sel.value; loadAnim(); };
  $$("#modeSeg button").forEach((b) => (b.onclick = () => {
    A.mode = b.dataset.mode;
    $$("#modeSeg button").forEach((x) => x.classList.toggle("on", x === b));
    $("#reelField").hidden = A.mode !== "reel";
    $("#animField").hidden = A.mode === "reel";
    $("#faceField").hidden = A.mode === "sync";
    loadAnim();
  }));
  $("#mirror").onchange = (e) => { A.mirror = e.target.checked; loadAnim(); };
  $("#speed").onchange = (e) => (A.speed = +e.target.value);
  $("#loop").onchange = (e) => (A.loop = e.target.checked);
  $("#bPlay").onclick = togglePlay;
  $("#bStart").onclick = () => seekFrame(0);
  $("#bPrev").onclick = () => { pause(); seekFrame(curFrame() - 1); };
  $("#bNext").onclick = () => { pause(); seekFrame(curFrame() + 1); };
  $("#scrub").oninput = (e) => { pause(); seekFrame(+e.target.value); };
  $("#reelGo").onclick = loadAnim;
  $("#exp1").onclick = () => exportFrame(1);
  $("#exp4").onclick = () => exportFrame(4);
  $("#expO").onclick = () => exportFrame(0);
  document.addEventListener("keydown", (e) => {
    if (!$("#tab-animate").classList.contains("on") || e.target.matches("input[type=number], textarea, select")) return;
    if (e.code === "Space") { e.preventDefault(); togglePlay(); }
    else if (e.code === "ArrowRight") { pause(); seekFrame(curFrame() + 1); }
    else if (e.code === "ArrowLeft") { pause(); seekFrame(curFrame() - 1); }
    else if (e.code === "Home") seekFrame(0);
  });
}

const primary = () => (A.mode === "sync" ? A.sync && A.sync.F : A.data);
const curFrame = () => { const d = primary(); return d ? Math.min(d.count - 1, Math.floor((A.t * d.fps) / 1000)) : 0; };

async function loadAnim() {
  pause();
  A.t = 0;
  const stage = $("#stageWrap");
  stage.innerHTML = '<p class="hint">Rendering with the firmware core…</p>';
  try {
    if (A.mode === "sync") {
      const all = await Promise.all(S.info.faces.map((f) => api(`/api/anim?id=${A.id}&face=${f.id}&fps=30&mirror=${+A.mirror}`)));
      A.sync = {};
      all.forEach((d, i) => { d.decoded = d.frames.map(decode); A.sync[S.info.faces[i].id] = d; });
      A.data = null;
      stage.className = "stage sync";
      stage.innerHTML = S.info.faces.map((f) => `<div><div class="bezel"><canvas data-face="${f.id}"></canvas></div><div class="lab">${f.id} · ${f.name}</div></div>`).join("");
    } else {
      const url = A.mode === "reel"
        ? `/api/reel?face=${A.face}&seconds=${+$("#reelSec").value || 30}&seed=${+$("#reelSeed").value || 0}&fps=30&first=2500`
        : `/api/anim?id=${A.id}&face=${A.face}&fps=30&mirror=${+A.mirror}`;
      A.data = await api(url);
      A.data.decoded = A.data.frames.map(decode);
      A.sync = null;
      stage.className = "stage";
      stage.innerHTML = '<div class="bezel"><canvas id="stageCanvas"></canvas></div>';
    }
  } catch (err) {
    stage.innerHTML = `<p class="hint">Could not render: ${err.message}</p>`;
    return;
  }
  const d = primary();
  const meta = S.info.animations.find((a) => a.id === A.id);
  $("#animSummary").textContent = A.mode === "reel" ? "Idle behaviour (blinks, saccades, breathing, micro-smiles) with autonomous playback; the first show is triggered at 2.5 s." : (meta ? meta.summary : "");
  $("#scrub").max = d.count - 1;
  buildMarks(d);
  drawAnimFrame();
  play();
}

function buildMarks(d) {
  const m = $("#marks");
  m.innerHTML = "";
  const dur = d.t[d.count - 1] || 1;
  let marks = d.markers || [];
  if (A.mode === "reel") {
    marks = [];
    for (let i = 1; i < d.count; i++) {
      if (d.playing[i] && !d.playing[i - 1]) marks.push({ t: d.t[i], label: `▶ #${d.playing[i]}` });
      if (!d.playing[i] && d.playing[i - 1]) marks.push({ t: d.t[i], label: "idle" });
    }
  }
  marks.forEach((k, i) => {
    const el = document.createElement("div");
    el.className = "mark";
    el.style.left = `${(k.t / dur) * 100}%`;
    el.innerHTML = `<span style="top:${i % 2 ? 14 : 0}px">${k.label}</span>`;
    el.title = `${k.t} ms · ${k.label}`;
    el.dataset.t = k.t;
    m.appendChild(el);
  });
}

function phaseAt(d, t) {
  if (A.mode === "reel") {
    const i = curFrame();
    return d.playing[i] ? `▶ #${d.playing[i]} ${S.info.animations.find((a) => a.id === d.playing[i])?.name || ""}` : "idle";
  }
  let label = "";
  (d.markers || []).forEach((m) => { if (m.t <= t) label = m.label; });
  return label;
}

function drawAnimFrame() {
  const d = primary();
  if (!d) return;
  const i = curFrame();
  if (A.mode === "sync") {
    $$("#stageWrap canvas").forEach((cv) => paint(cv, A.sync[cv.dataset.face].decoded[Math.min(i, A.sync[cv.dataset.face].count - 1)], { scale: 3 }));
  } else {
    const cv = $("#stageCanvas");
    if (cv) paint(cv, d.decoded[i], { scale: 6 });
  }
  const buf = (A.mode === "sync" ? A.sync.F : d).decoded[i];
  paintNative($("#trueCanvas"), buf, 1);
  $("#scrub").value = i;
  const t = d.t[i];
  $("#readout").textContent = `${t} ms · frame ${i}/${d.count - 1}`;
  $("#phase").textContent = phaseAt(d, t);
  $$("#marks .mark").forEach((m) => m.classList.toggle("cur", +m.dataset.t <= t));
}

let raf = 0, last = 0;
function loop(ts) {
  const d = primary();
  if (!A.playing || !d) return;
  const dt = last ? ts - last : 0;
  last = ts;
  A.t += dt * A.speed;
  const end = (d.count * 1000) / d.fps;
  if (A.t >= end) {
    if (A.loop) A.t = A.t % end;
    else { A.t = end - 1; pause(); }
  }
  drawAnimFrame();
  raf = requestAnimationFrame(loop);
}
function play() { if (A.playing) return; A.playing = true; last = 0; $("#bPlay").textContent = "⏸ Pause"; raf = requestAnimationFrame(loop); }
function pause() { A.playing = false; cancelAnimationFrame(raf); $("#bPlay").textContent = "▶ Play"; }
function togglePlay() { A.playing ? pause() : play(); }
function seekFrame(i) {
  const d = primary();
  if (!d) return;
  i = Math.max(0, Math.min(d.count - 1, i));
  A.t = (i * 1000) / d.fps + 0.5;
  drawAnimFrame();
}

function exportFrame(kind) {
  const d = primary();
  if (!d) return;
  const i = curFrame();
  const face = A.mode === "sync" ? "F" : A.face;
  const buf = (A.mode === "sync" ? A.sync.F : d).decoded[i];
  const cv = document.createElement("canvas");
  const tag = A.mode === "reel" ? "reel" : `anim${A.id}`;
  if (kind === 0) paint(cv, buf, { scale: 6 });
  else paintNative(cv, buf, kind);
  download(cv, `tinybuddy_${tag}_${face}_${d.t[i]}ms${kind === 0 ? "_oled" : `_x${kind}`}.png`);
}

// ---------------------------------------------------------------- TUNE
const TUNE_EXPRS = ["rest", "blink", "happy", "joy", "surprised", "sad", "angry", "sleepy", "look_left"];

async function initTune() {
  S.tune.params = await api("/api/params");
  S.tune.styles = await api("/api/styles");
  const chips = $("#tuneFaceChips");
  S.info.faces.forEach((f) => {
    const b = document.createElement("button");
    b.textContent = `${f.id} · ${f.name}`;
    b.dataset.face = f.id;
    b.className = f.id === S.tune.face ? "on" : "";
    b.onclick = () => { S.tune.face = f.id; S.tune.sets = {}; $$("#tuneFaceChips button").forEach((x) => x.classList.toggle("on", x === b)); buildTuneParams(); renderTune(); };
    chips.appendChild(b);
  });
  $("#tunePreview").innerHTML = TUNE_EXPRS.map((e) => `<div><div class="bezel"><canvas data-expr="${e}"></canvas></div><div class="lab">${e === "rest" ? "resting" : EXPR_LABEL(e)}</div></div>`).join("");
  $("#tuneReset").onclick = () => { S.tune.sets = {}; buildTuneParams(); renderTune(); };
  $("#tuneApprove").onclick = () =>
    confirmCanonical(S.tune.face, "", Object.entries(S.tune.sets).map(([k, v]) => `${S.tune.face}:${k}=${v}`));
  buildTuneParams();
  renderTune();
}

function buildTuneParams() {
  const box = $("#tuneParams");
  const st = S.tune.styles[S.tune.face];
  box.innerHTML = "";
  S.tune.params.forEach((p) => {
    const row = document.createElement("div");
    row.className = "prm";
    const val = S.tune.sets[p.name] ?? st[p.name];
    if (p.options) {
      row.innerHTML = `<label title="${p.name}">${p.name}</label><select>${p.options.map((o) => `<option ${o === val ? "selected" : ""}>${o}</option>`).join("")}</select><output></output>`;
      $("select", row).onchange = (e) => setParam(p.name, e.target.value, row);
    } else if (p.type === 1) {
      row.innerHTML = `<label title="${p.name}">${p.name}</label><input type="checkbox" ${val === "1" ? "checked" : ""}><output></output>`;
      $("input", row).onchange = (e) => setParam(p.name, e.target.checked ? "1" : "0", row);
    } else {
      const step = p.hi - p.lo > 10 ? 0.5 : 0.01;
      row.innerHTML = `<label title="${p.name}">${p.name}</label><input type="range" min="${p.lo}" max="${p.hi}" step="${step}" value="${val}"><output>${(+val).toFixed(step < 0.1 ? 2 : 1)}</output>`;
      $("input", row).oninput = (e) => { $("output", row).textContent = (+e.target.value).toFixed(step < 0.1 ? 2 : 1); setParam(p.name, e.target.value, row); };
    }
    row.classList.toggle("changed", p.name in S.tune.sets);
    box.appendChild(row);
  });
}

let tuneTimer = 0;
function setParam(name, value, row) {
  S.tune.sets[name] = String(+value === +value && value !== "" && !isNaN(+value) ? +(+value).toFixed(3) : value);
  row.classList.add("changed");
  clearTimeout(tuneTimer);
  tuneTimer = setTimeout(renderTune, 90);
}

async function renderTune() {
  if (!S.tune.params.length) return;
  const sets = Object.entries(S.tune.sets).map(([k, v]) => `${S.tune.face}:${k}=${v}`);
  $("#tuneOut").value = sets.length ? sets.map((s) => `--set ${s}`).join(" ") : "(no tweaks — this is the round-5 design)";
  await Promise.all(TUNE_EXPRS.map(async (e) => {
    const buf = await pose(S.tune.face, e, sets);
    const cv = $(`#tunePreview canvas[data-expr="${e}"]`);
    if (cv) paint(cv, buf, { scale: 3 });
  }));
}

// ---------------------------------------------------------------- HISTORY
async function loadRounds() {
  const box = $("#rounds");
  if (box.dataset.loaded) return;
  const rounds = await api("/api/rounds");
  box.dataset.loaded = 1;
  box.innerHTML = rounds.slice().reverse().map((r) => `
    <div class="round">
      <h3>Round ${r.n}${r.n === rounds.length ? " · current" : ""}</h3>
      <a href="${r.sheet}" target="_blank"><img src="${r.sheet}" alt="Round ${r.n} contact sheet" loading="lazy"></a>
      ${r.notes ? `<pre>${r.notes.replace(/[<&]/g, (c) => (c === "<" ? "&lt;" : "&amp;"))}</pre>` : ""}
      <div class="ex">${r.expressions.map((p) => `<a href="${p}" target="_blank">${p.split("/").pop().replace(".png", "").replace("_", " ")}</a>`).join("")}</div>
    </div>`).join("");
}

// ---------------------------------------------------------------- logo: a tiny living TinyBuddy
async function logo() {
  const cv = $("#logo");
  const open = await pose("F", "rest"), shut = await pose("F", "blink"), joy = await pose("F", "joy");
  let n = 0;
  const tick = () => {
    n++;
    const f = n % 9 === 0 ? joy : (n % 3 === 0 ? shut : open);
    paintNative(cv, f, 1);
    setTimeout(() => { paintNative(cv, open, 1); }, f === shut ? 140 : 900);
  };
  paintNative(cv, open, 1);
  setInterval(tick, 1600);
}

// ---------------------------------------------------------------- boot
(async function boot() {
  try {
    S.info = await api("/api/info");
    const r = await api("/api/reviews");
    S.reviews = r;
    S.approved = r.approved;
  } catch (e) {
    document.body.insertAdjacentHTML("afterbegin", `<p style="padding:16px;color:#ff7676">Server error: ${e.message}</p>`);
    return;
  }
  buildFaces();
  buildAnimate();
  logo();
  const tab = localStore("tabname");
  if (tab && tab !== "faces") showTab(tab);
})();
