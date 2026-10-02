import { renderGantt } from './gantt.js';
import { PRESETS, LIMITS, randomWorkload, validate } from './workload.js';
import { colorOf, esc, fmt, pad2, reducedMotion } from './util.js';

const $ = (id) => document.getElementById(id);
const TICK_MS = 700;
const TOKEN = 40;
const GAP = 8;

export function initSimulator(engine) {
  const state = {
    processes: structuredClone(PRESETS.textbook.processes),
    alg: engine.catalog.find((a) => a.short === 'SRT') ?? engine.catalog[0],
    param: 0,
    run: null,
    tick: 0,
    timer: null,
    speed: 1,
  };
  state.param = state.alg.paramDefault;
  const listeners = [];

  // ---------- workload editor ----------
  const presetSelect = $('preset');
  presetSelect.innerHTML =
    Object.entries(PRESETS).map(([key, p]) => `<option value="${key}">${esc(p.label)}</option>`).join('') +
    '<option value="custom" hidden>Custom</option>';

  function renderRows() {
    $('proc-rows').innerHTML = state.processes
      .map(
        (p, i) => `<tr data-i="${i}">
          <td><span class="swatch" style="background:${colorOf(i)}"></span></td>
          <td><input data-k="name" value="${esc(p.name)}" maxlength="4" aria-label="Process ${i + 1} name" /></td>
          <td><input data-k="arrival" type="number" min="0" max="${LIMITS.maxArrival}" value="${p.arrival}" aria-label="Process ${i + 1} arrival time" /></td>
          <td><input data-k="burst" type="number" min="1" max="${LIMITS.maxBurst}" value="${p.burst}" aria-label="Process ${i + 1} burst time" /></td>
          <td><input data-k="priority" type="number" min="0" max="${LIMITS.maxPriority}" value="${p.priority ?? ''}" placeholder="–" aria-label="Process ${i + 1} priority" /></td>
          <td><button class="row-remove" type="button" aria-label="Remove process ${esc(p.name)}" ${state.processes.length < 2 ? 'disabled' : ''}>×</button></td>
        </tr>`,
      )
      .join('');
    $('add-proc').disabled = state.processes.length >= LIMITS.maxProcesses;
  }

  function setWorkload(processes, presetKey = 'custom') {
    state.processes = processes;
    presetSelect.value = presetKey;
    renderRows();
    simulate({ resetTick: true });
  }

  presetSelect.addEventListener('change', () => setWorkload(structuredClone(PRESETS[presetSelect.value].processes), presetSelect.value));
  $('random-proc').addEventListener('click', () => setWorkload(randomWorkload()));
  $('add-proc').addEventListener('click', () => {
    const used = new Set(state.processes.map((p) => p.name));
    let code = 65;
    while (used.has(String.fromCharCode(code))) code++;
    const last = state.processes.at(-1);
    state.processes.push({ name: String.fromCharCode(code), arrival: Math.min((last?.arrival ?? 0) + 1, LIMITS.maxArrival), burst: 3 });
    setWorkload(state.processes);
  });

  $('proc-rows').addEventListener('click', (e) => {
    const btn = e.target.closest('.row-remove');
    if (!btn) return;
    state.processes.splice(Number(btn.closest('tr').dataset.i), 1);
    setWorkload(state.processes);
  });

  let editTimer = null;
  $('proc-rows').addEventListener('input', (e) => {
    const input = e.target;
    const proc = state.processes[Number(input.closest('tr').dataset.i)];
    const key = input.dataset.k;
    if (key === 'name') proc.name = input.value.trim();
    else if (key === 'priority' && input.value === '') delete proc.priority;
    else proc[key] = Number(input.value);
    presetSelect.value = 'custom';
    clearTimeout(editTimer);
    editTimer = setTimeout(() => simulate({ resetTick: true }), 250);
  });

  // ---------- algorithm picker ----------
  const picker = $('alg-picker');
  picker.innerHTML = engine.catalog
    .map((a) => `<button type="button" class="alg-option" role="radio" data-id="${a.id}" title="${esc(a.name)}">${esc(a.short)}</button>`)
    .join('');

  function renderAlgorithm() {
    for (const btn of picker.children) {
      const on = Number(btn.dataset.id) === state.alg.id;
      btn.setAttribute('aria-checked', String(on));
      btn.tabIndex = on ? 0 : -1;
    }
    const a = state.alg;
    $('param-field').hidden = !a.paramName;
    if (a.paramName) {
      $('param-label').textContent = a.paramName;
      $('param-input').value = state.param;
    }
    $('alg-card').innerHTML = `
      <h4>${esc(a.name)}</h4>
      <span class="badge">${esc(a.family)} · ${a.preemptive ? 'preemptive' : 'non-preemptive'}</span>
      <p class="rule">Greedy rule: ${esc(a.greedyRule)}</p>
      <p>${esc(a.description)}</p>`;
  }

  function setAlgorithm(id, param) {
    state.alg = engine.catalog.find((a) => a.id === id) ?? state.alg;
    state.param = param ?? state.alg.paramDefault;
    renderAlgorithm();
    simulate({ resetTick: true });
  }

  picker.addEventListener('click', (e) => {
    const btn = e.target.closest('.alg-option');
    if (btn) setAlgorithm(Number(btn.dataset.id));
  });
  picker.addEventListener('keydown', (e) => {
    const dir = { ArrowRight: 1, ArrowDown: 1, ArrowLeft: -1, ArrowUp: -1 }[e.key];
    if (!dir) return;
    e.preventDefault();
    const idx = engine.catalog.indexOf(state.alg);
    const next = engine.catalog[(idx + dir + engine.catalog.length) % engine.catalog.length];
    setAlgorithm(next.id);
    picker.querySelector(`[data-id="${next.id}"]`).focus();
  });
  $('param-input').addEventListener('change', (e) => {
    const value = Math.round(Number(e.target.value));
    state.param = Math.min(Math.max(Number.isFinite(value) ? value : 1, 1), 20);
    e.target.value = state.param;
    simulate({ resetTick: true });
  });

  // ---------- simulation ----------
  function simulate({ resetTick = false } = {}) {
    const error = validate(state.processes);
    $('workload-error').textContent = error ?? '';
    if (error) return;
    try {
      const out = engine.simulate(state.processes, [{ id: state.alg.id, param: state.alg.paramName ? state.param : 0 }]);
      state.run = out.runs[0];
    } catch (err) {
      $('workload-error').textContent = err.message;
      return;
    }
    stop();
    if (resetTick) state.tick = 0;
    state.tick = Math.min(state.tick, makespan());
    $('ctl-scrub').max = makespan();
    buildTokens();
    renderResults();
    renderDecisions();
    renderTick();
    listeners.forEach((fn) => fn(state.processes));
  }

  const makespan = () => state.run.summary.makespan;
  const charAt = (p, t) => state.run.timeline[p]?.[t] ?? ' ';
  const runningAt = (t) => state.processes.findIndex((_, p) => charAt(p, t) === '*');

  // ---------- stage ----------
  const tokensHost = $('tokens');
  const stage = $('stage');

  function buildTokens() {
    tokensHost.innerHTML = state.processes
      .map((p, i) => `<div class="token" data-p="${i}" style="background:${colorOf(i)}">${esc(p.name)}<span class="rem"></span></div>`)
      .join('');
  }

  function placeTokens() {
    const t = state.tick;
    const base = stage.getBoundingClientRect();
    const rectOf = (sel) => {
      const r = stage.querySelector(sel).getBoundingClientRect();
      return { x: r.left - base.left, y: r.top - base.top, w: r.width, h: r.height };
    };
    const lanes = { incoming: rectOf('.lane-incoming'), ready: rectOf('.lane-ready'), done: rectOf('.lane-done'), cpu: rectOf('.cpu') };
    const groups = { incoming: [], ready: [], done: [], cpu: [] };

    state.processes.forEach((proc, p) => {
      const finish = state.run.metrics[p].finish;
      let where;
      if (proc.arrival > t) where = 'incoming';
      else if (finish > 0 && finish <= t) where = 'done';
      else if (charAt(p, t) === '*') where = 'cpu';
      else where = 'ready';
      groups[where].push(p);
    });

    // Ready queue is shown in the order processes (re)joined it, which mirrors FIFO for RR.
    const joinedAt = (p) => {
      let s = t;
      while (s > 0 && charAt(p, s - 1) === '.') s--;
      return s;
    };
    groups.ready.sort((a, b) => joinedAt(a) - joinedAt(b) || a - b);
    groups.incoming.sort((a, b) => state.processes[a].arrival - state.processes[b].arrival);

    const positions = new Map();
    for (const key of ['incoming', 'ready', 'done']) {
      const lane = lanes[key];
      const perRow = Math.max(1, Math.floor((lane.w - 20 + GAP) / (TOKEN + GAP)));
      groups[key].forEach((p, i) => {
        positions.set(p, { x: lane.x + 10 + (i % perRow) * (TOKEN + GAP), y: lane.y + 30 + Math.floor(i / perRow) * (TOKEN + GAP) });
      });
    }
    for (const p of groups.cpu) positions.set(p, { x: lanes.cpu.x + lanes.cpu.w / 2 - 32, y: lanes.cpu.y + lanes.cpu.h / 2 - 32 });

    for (const el of tokensHost.children) {
      const p = Number(el.dataset.p);
      const pos = positions.get(p);
      const ran = [...(state.run.timeline[p] ?? '').slice(0, t)].filter((c) => c === '*').length;
      el.classList.toggle('running', groups.cpu.includes(p));
      el.classList.toggle('done', groups.done.includes(p));
      el.style.transform = `translate(${pos.x}px, ${pos.y}px)`;
      el.querySelector('.rem').textContent = groups.done.includes(p) ? '✓' : state.processes[p].burst - ran;
      el.title = `${state.processes[p].name}: ${state.processes[p].burst - ran} of ${state.processes[p].burst} ticks left`;
    }
    stage.querySelector('.cpu').classList.toggle('busy', groups.cpu.length > 0);
  }

  function tickEvents(t) {
    const events = [];
    state.processes.forEach((p, i) => {
      if (p.arrival === t) events.push(`${p.name} arrives`);
      if (state.run.metrics[i].finish > 0 && state.run.metrics[i].finish === t) events.push(`${p.name} finishes`);
    });
    if (t > 0 && t < makespan()) {
      const prev = runningAt(t - 1);
      const now = runningAt(t);
      if (prev >= 0 && now >= 0 && prev !== now) events.push(`switch ${state.processes[prev].name}→${state.processes[now].name}`);
    }
    return events.join(' · ');
  }

  function renderWhy(t) {
    const why = $('why');
    if (t >= makespan()) {
      const s = state.run.summary;
      why.innerHTML = state.run.completes
        ? `<span class="tag">DONE</span><span>All ${state.processes.length} processes finished at t=${makespan()} — average waiting <b>${fmt(s.avgWaiting)}</b>, ${s.contextSwitches} context switches.</span>`
        : `<span class="tag">END</span><span>${esc(state.run.label)} models a long-running system: processes keep competing until the horizon, so nobody “finishes”. Note how every process still gets CPU time — no starvation.</span>`;
      return;
    }
    const now = runningAt(t);
    if (now < 0) {
      why.innerHTML = '<span class="tag idle">IDLE</span><span>No process is ready, so the CPU sits idle until the next arrival.</span>';
      return;
    }
    const decisions = state.run.decisions;
    const exact = decisions.find((d) => d.t === t);
    if (exact) {
      why.innerHTML = `<span class="tag">t=${t}</span><span>Dispatch <b>${esc(state.processes[exact.p].name)}</b> — ${esc(exact.reason)}</span>`;
      return;
    }
    const last = decisions.filter((d) => d.t <= t).at(-1);
    why.innerHTML = `<span class="tag">t=${t}</span><span><b>${esc(state.processes[now].name)}</b> keeps the CPU${last ? ` (dispatched at t=${last.t})` : ''} — no event forces a new decision this tick.</span>`;
  }

  function renderTick() {
    const t = state.tick;
    $('clock-t').textContent = `t = ${pad2(t)}`;
    $('clock-span').textContent = `/ ${pad2(makespan())}`;
    $('ctl-scrub').value = t;
    $('tick-events').textContent = tickEvents(t);
    placeTokens();
    renderWhy(t);
    renderGantt($('sim-gantt'), state.processes, state.run, { upto: t, ghost: true });
    for (const li of $('decisions').children) {
      const dt = Number(li.dataset.t);
      li.classList.toggle('future', dt > t);
      li.classList.toggle('current', li === lastDecisionItem(t));
    }
  }

  const lastDecisionItem = (t) => [...$('decisions').children].filter((li) => Number(li.dataset.t) <= t).at(-1);

  // ---------- results ----------
  function renderResults() {
    const s = state.run.summary;
    const label = state.run.label;
    const na = (v) => (state.run.completes ? v : '—');
    $('result-note').textContent = `${label} · ${state.processes.length} processes · ${makespan()} ticks${state.run.completes ? '' : ' · never completes'}`;
    const cards = [
      ['Avg waiting', na(fmt(s.avgWaiting))],
      ['Avg turnaround', na(fmt(s.avgTurnaround))],
      ['Avg response', fmt(s.avgResponse)],
      ['CPU utilisation', `${fmt(s.cpuUtilization * 100, 0)}%`],
      ['Context switches', s.contextSwitches],
      ['Fairness (Jain)', na(fmt(s.fairness))],
    ];
    $('summary').innerHTML = cards.map(([k, v]) => `<div><dt>${k}</dt><dd class="mono">${v}</dd></div>`).join('');

    const rows = state.processes
      .map((p, i) => {
        const m = state.run.metrics[i];
        if (m.finish <= 0) return `<tr><td><span class="swatch" style="background:${colorOf(i)}"></span> ${esc(p.name)}</td><td>${p.arrival}</td><td>${p.burst}</td><td>—</td><td>—</td><td>—</td><td>${m.waiting}</td><td>${m.response >= 0 ? m.response : '—'}</td></tr>`;
        return `<tr><td><span class="swatch" style="background:${colorOf(i)}"></span> ${esc(p.name)}</td><td>${p.arrival}</td><td>${p.burst}</td><td>${m.finish}</td><td>${m.turnaround}</td><td>${fmt(m.normTurn)}</td><td>${m.waiting}</td><td>${m.response}</td></tr>`;
      })
      .join('');
    const foot = state.run.completes
      ? `<tr><td>Mean</td><td></td><td></td><td></td><td>${fmt(s.avgTurnaround)}</td><td>${fmt(s.avgNormTurn)}</td><td>${fmt(s.avgWaiting)}</td><td>${fmt(s.avgResponse)}</td></tr>`
      : '';
    $('metrics-table').innerHTML = `
      <thead><tr><th>Process</th><th>Arrival</th><th>Burst</th><th>Finish</th><th>Turnaround</th><th>Norm. TAT</th><th>Waiting</th><th>Response</th></tr></thead>
      <tbody>${rows}</tbody>
      <tfoot>${foot}</tfoot>`;
  }

  function renderDecisions() {
    $('decisions').innerHTML = state.run.decisions
      .map((d) => `<li data-t="${d.t}" tabindex="0"><b>t=${pad2(d.t)}</b> ${esc(state.processes[d.p].name)} — ${esc(d.reason)}</li>`)
      .join('');
  }

  $('decisions').addEventListener('click', (e) => {
    const li = e.target.closest('li');
    if (li) goTo(Number(li.dataset.t));
  });
  $('decisions').addEventListener('keydown', (e) => {
    if (e.key === 'Enter' && e.target.matches('li')) goTo(Number(e.target.dataset.t));
  });

  // ---------- playback ----------
  const playBtn = $('ctl-play');

  function goTo(t) {
    state.tick = Math.min(Math.max(t, 0), makespan());
    renderTick();
  }

  function stop() {
    clearInterval(state.timer);
    state.timer = null;
    playBtn.textContent = '▶';
    playBtn.setAttribute('aria-label', 'Play');
  }

  function play() {
    if (!state.run) return;
    if (state.tick >= makespan()) goTo(0);
    playBtn.textContent = '❚❚';
    playBtn.setAttribute('aria-label', 'Pause');
    clearInterval(state.timer);
    state.timer = setInterval(() => {
      if (state.tick >= makespan()) return stop();
      goTo(state.tick + 1);
    }, TICK_MS / state.speed);
  }

  playBtn.addEventListener('click', () => (state.timer ? stop() : play()));
  $('ctl-step').addEventListener('click', () => { stop(); goTo(state.tick + 1); });
  $('ctl-back').addEventListener('click', () => { stop(); goTo(state.tick - 1); });
  $('ctl-reset').addEventListener('click', () => { stop(); goTo(0); });
  $('ctl-scrub').addEventListener('input', (e) => { stop(); goTo(Number(e.target.value)); });
  $('ctl-speed').addEventListener('change', (e) => {
    state.speed = Number(e.target.value);
    if (state.timer) play();
  });

  document.addEventListener('keydown', (e) => {
    if (e.target.closest('input, select, textarea, button, [role="radiogroup"], li') || e.metaKey || e.ctrlKey || e.altKey) return;
    const sim = $('simulator').getBoundingClientRect();
    if (sim.bottom < 0 || sim.top > window.innerHeight) return;
    const actions = {
      ' ': () => (state.timer ? stop() : play()),
      ArrowRight: () => { stop(); goTo(state.tick + 1); },
      ArrowLeft: () => { stop(); goTo(state.tick - 1); },
      Home: () => { stop(); goTo(0); },
      End: () => { stop(); goTo(makespan()); },
    };
    if (actions[e.key]) {
      e.preventDefault();
      actions[e.key]();
    }
  });

  // ---------- boot ----------
  renderRows();
  renderAlgorithm();
  simulate();
  if (reducedMotion()) tokensHost.classList.add('no-motion');

  return {
    setAlgorithm(id, param) {
      setAlgorithm(id, param);
      $('simulator').scrollIntoView({ behavior: reducedMotion() ? 'auto' : 'smooth', block: 'start' });
    },
    onChange(fn) {
      listeners.push(fn);
      fn(state.processes);
    },
    relayout() {
      if (state.run) renderTick();
    },
  };
}
