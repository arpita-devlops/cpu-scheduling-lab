import { esc, fmt, pct } from './util.js';

const $ = (id) => document.getElementById(id);

export function initBenchmark(engine) {
  const form = $('bench-form');

  function run() {
    const runs = Number($('bench-runs').value);
    const seed = Math.max(0, Math.floor(Number($('bench-seed').value) || 0));
    const profile = $('bench-profile').value;
    $('bench-run').disabled = true;
    $('bench-time').textContent = 'running…';

    // Yield a frame so the "running…" state paints before the synchronous WASM call.
    requestAnimationFrame(() =>
      setTimeout(() => {
        try {
          const started = performance.now();
          const data = engine.benchmark(runs, seed, profile);
          const ms = performance.now() - started;
          const schedules = runs * data.algorithms.length;
          $('bench-time').textContent = `${schedules.toLocaleString()} schedules in ${fmt(ms, 0)} ms`;
          render(data);
        } catch (err) {
          $('bench-time').textContent = `failed: ${err.message}`;
        } finally {
          $('bench-run').disabled = false;
        }
      }),
    );
  }

  function render(data) {
    const algs = data.algorithms;
    const reduction = (a) => a.metrics.waitingReduction.mean;
    const maxAbs = Math.max(...algs.map((a) => Math.abs(reduction(a))), 0.01);

    $('bench-chart').innerHTML = [...algs]
      .sort((a, b) => reduction(b) - reduction(a))
      .map((a) => {
        const r = reduction(a);
        const width = (Math.abs(r) / maxAbs) * 50;
        const cls = r >= 0 ? 'pos' : 'neg';
        const left = r >= 0 ? 50 : 50 - width;
        return `<div class="bar-row" title="${esc(a.label)}: ${pct(r)} average waiting vs FCFS">
          <span>${esc(a.label)}</span>
          <span class="bar-track"><span class="bar ${cls}" style="left:${left}%;width:${width}%"></span></span>
          <span class="bar-value ${cls}">${a.label === 'FCFS' ? 'base' : pct(r)}</span></div>`;
      })
      .join('');

    const mean = (a, k) => a.metrics[k].mean;
    const by = (k, dir = 1) => [...algs].sort((a, b) => dir * (mean(a, k) - mean(b, k)))[0];
    const wait = by('waitingReduction', -1);
    const resp = by('avgResponse');
    const fair = by('fairness', -1);
    const switches = by('contextSwitches', -1);
    $('bench-findings').innerHTML = [
      `<li><b>${esc(wait.label)}</b> cuts average waiting by <b>${fmt(mean(wait, 'waitingReduction') * 100, 1)}%</b> vs FCFS (p95 ${fmt(wait.metrics.waitingReduction.p95 * 100, 0)}%).</li>`,
      `<li><b>${esc(resp.label)}</b> has the fastest average response — ${fmt(mean(resp, 'avgResponse'))} ticks to first run.</li>`,
      `<li><b>${esc(fair.label)}</b> is the fairest (Jain index ${fmt(mean(fair, 'fairness'))}).</li>`,
      `<li>The price: <b>${esc(switches.label)}</b> pays ${fmt(mean(switches, 'contextSwitches'), 1)} context switches per workload.</li>`,
    ].join('');

    const cols = [
      ['avgWaiting', 'Avg wait', 2],
      ['avgTurnaround', 'Avg TAT', 2],
      ['avgResponse', 'Avg response', 2],
      ['contextSwitches', 'Switches', 1],
      ['fairness', 'Fairness', 2],
      ['cpuUtilization', 'CPU', 2],
    ];
    const best = Object.fromEntries(
      cols.map(([k]) => {
        const v = algs.map((a) => mean(a, k));
        return [k, k === 'fairness' || k === 'cpuUtilization' ? Math.max(...v) : Math.min(...v)];
      }),
    );
    $('bench-table').innerHTML = `
      <thead><tr><th>Algorithm</th>${cols.map(([, l]) => `<th>${l} <small>mean</small></th>`).join('')}<th>Wait p95</th><th>vs FCFS</th></tr></thead>
      <tbody>${algs
        .map(
          (a) => `<tr><td>${esc(a.label)}</td>${cols
            .map(([k, , d]) => {
              const v = mean(a, k);
              const text = k === 'cpuUtilization' ? `${fmt(v * 100, 0)}%` : fmt(v, d);
              return `<td class="${Math.abs(v - best[k]) < 1e-9 ? 'best' : ''}">${text}</td>`;
            })
            .join('')}<td>${fmt(a.metrics.avgWaiting.p95)}</td><td>${a.label === 'FCFS' ? '—' : pct(mean(a, 'waitingReduction'))}</td></tr>`,
        )
        .join('')}</tbody>`;

    return wait;
  }

  form.addEventListener('submit', (e) => {
    e.preventDefault();
    run();
  });

  // Headline numbers on the page come from a live run, not hard-coded text.
  const headline = engine.benchmark(1000, 42, 'mixed');
  const top = render(headline);
  const value = top.metrics.waitingReduction.mean * 100;
  $('stat-reduction').innerHTML = `${fmt(value, 0)}%<small> ${esc(top.label)}</small>`;
  $('stat-reduction-note').textContent = 'vs FCFS · 1,000 random workloads · seed 42';
  $('claim-reduction').textContent = `${fmt(value, 1)}% less average waiting than FCFS (${top.label}, 1,000 seeded workloads)`;
  $('bench-time').textContent = '10,000 schedules · computed in your browser';
}
