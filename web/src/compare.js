import { renderGantt } from './gantt.js';
import { esc, fmt } from './util.js';

const $ = (id) => document.getElementById(id);

const COLUMNS = [
  { key: 'avgWaiting', label: 'Avg wait', better: 'min', needsCompletion: true },
  { key: 'avgTurnaround', label: 'Avg TAT', better: 'min', needsCompletion: true },
  { key: 'avgResponse', label: 'Avg response', better: 'min' },
  { key: 'contextSwitches', label: 'Switches', better: 'min', digits: 0 },
  { key: 'fairness', label: 'Fairness', better: 'max', needsCompletion: true },
  { key: 'cpuUtilization', label: 'CPU', better: 'max', percent: true },
];

const applies = (run, column) => run.completes || !column.needsCompletion;

export function initCompare(engine, simulator) {
  const specs = engine.catalog.map((a) => ({ id: a.id, param: a.paramName ? a.paramDefault : 0 }));
  const rr = engine.catalog.find((a) => a.short === 'RR');
  if (rr) specs.splice(specs.findIndex((s) => s.id === rr.id) + 1, 0, { id: rr.id, param: 4 });

  let latest = null;

  function render(processes) {
    let out;
    try {
      out = engine.simulate(processes, specs);
    } catch {
      return;
    }
    latest = { processes, runs: out.runs };

    const best = Object.fromEntries(
      COLUMNS.map((c) => {
        const values = out.runs.filter((r) => applies(r, c)).map((r) => r.summary[c.key]);
        return [c.key, c.better === 'min' ? Math.min(...values) : Math.max(...values)];
      }),
    );

    const head = `<thead><tr><th>Algorithm</th><th>Schedule</th>${COLUMNS.map((c) => `<th>${c.label}</th>`).join('')}</tr></thead>`;
    const body = out.runs
      .map((run, i) => {
        const meta = engine.catalog.find((a) => a.id === run.id);
        const cells = COLUMNS.map((c) => {
          if (!applies(run, c)) return '<td title="Processes never complete under this model">—</td>';
          const v = run.summary[c.key];
          const text = c.percent ? `${fmt(v * 100, 0)}%` : fmt(v, c.digits ?? 2);
          return `<td class="${Math.abs(v - best[c.key]) < 1e-9 ? 'best' : ''}">${text}</td>`;
        }).join('');
        return `<tr data-i="${i}" tabindex="0" title="Open ${esc(run.label)} in the simulator">
          <td class="alg-name">${esc(run.label)}<small>${esc(meta?.name ?? '')}${run.completes ? '' : ' · never completes'}</small></td>
          <td class="mini" data-gantt="${i}"></td>${cells}</tr>`;
      })
      .join('');
    $('compare-table').innerHTML = head + `<tbody>${body}</tbody>`;
    drawMinis();
    $('compare-insight').innerHTML = insight(out.runs);
  }

  function drawMinis() {
    if (!latest) return;
    for (const cell of $('compare-table').querySelectorAll('[data-gantt]')) {
      renderGantt(cell, latest.processes, latest.runs[Number(cell.dataset.gantt)], { mini: true });
    }
  }

  function insight(allRuns) {
    const runs = allRuns.filter((r) => r.completes);
    const by = (key, dir = 1) => [...runs].sort((a, b) => dir * (a.summary[key] - b.summary[key]))[0];
    const fcfs = runs.find((r) => r.label === 'FCFS');
    const wait = by('avgWaiting');
    const resp = by('avgResponse');
    const fair = by('fairness', -1);
    const parts = [];
    if (fcfs && fcfs.summary.avgWaiting > 0 && wait.label !== 'FCFS') {
      const cut = (1 - wait.summary.avgWaiting / fcfs.summary.avgWaiting) * 100;
      parts.push(`<b>${esc(wait.label)}</b> has the lowest average waiting time (${fmt(wait.summary.avgWaiting)}) — <b>${fmt(cut, 0)}% less than FCFS</b>.`);
    } else {
      parts.push(`On this workload no algorithm beats FCFS on waiting time (${fmt(wait.summary.avgWaiting)}).`);
    }
    parts.push(`<b>${esc(resp.label)}</b> responds fastest (${fmt(resp.summary.avgResponse)} ticks to first run)`);
    parts.push(`and <b>${esc(fair.label)}</b> spreads slowdown most evenly (fairness ${fmt(fair.summary.fairness)}).`);
    return `${parts[0]} ${parts[1]}, ${parts[2]} Click a row to replay it.`;
  }

  const table = $('compare-table');
  const open = (row) => {
    const run = latest.runs[Number(row.dataset.i)];
    simulator.setAlgorithm(run.id, run.param || undefined);
  };
  table.addEventListener('click', (e) => {
    const row = e.target.closest('tbody tr');
    if (row) open(row);
  });
  table.addEventListener('keydown', (e) => {
    const row = e.target.closest('tbody tr');
    if (row && (e.key === 'Enter' || e.key === ' ')) {
      e.preventDefault();
      open(row);
    }
  });

  simulator.onChange(render);
  return { relayout: drawMinis };
}
