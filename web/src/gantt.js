import { colorOf, esc, uid } from './util.js';

const NS = 'http://www.w3.org/2000/svg';

/**
 * SVG Gantt chart: a CPU lane (who holds the CPU) plus one row per process
 * (solid = running, hatched = waiting in the ready queue).
 *
 * options.upto  – reveal ticks [0, upto); Infinity shows everything
 * options.ghost – draw not-yet-revealed ticks faintly (preview of the future)
 * options.mini  – CPU lane only, no axis (comparison table)
 */
export function renderGantt(host, processes, run, { upto = Infinity, ghost = false, mini = false } = {}) {
  const cols = Math.max(run.summary.makespan, ...processes.map((p) => p.arrival + 1), 1);
  const labelW = mini ? 0 : 52;
  const available = Math.max(host.clientWidth || 600, 240) - labelW - 8;
  const cell = mini ? Math.max(available / cols, 4) : Math.max(Math.min(available / cols, 42), 16);
  const laneH = mini ? 20 : 30;
  const rowH = 22;
  const axisH = mini ? 0 : 20;
  const rows = mini ? 0 : processes.length;
  const width = labelW + cols * cell + 8;
  const height = axisH + laneH + (rows ? 10 + rows * rowH : 0) + 4;
  const hatch = uid('hatch');
  const reveal = Math.min(upto, cols);

  const parts = [
    `<svg xmlns="${NS}" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}" role="img" aria-label="Gantt chart">`,
    `<defs><pattern id="${hatch}" width="5" height="5" patternUnits="userSpaceOnUse" patternTransform="rotate(45)"><rect width="1.6" height="5" fill="#b9b5a8"/></pattern></defs>`,
  ];

  const x = (t) => labelW + t * cell;
  const opacityAt = (t) => (t < reveal ? 1 : ghost ? 0.13 : 0);

  if (!mini) {
    const step = cell >= 22 ? 1 : cell >= 12 ? 2 : 5;
    for (let t = 0; t <= cols; t += step) {
      parts.push(`<text x="${x(t)}" y="12" font-size="10" fill="#6b7280" text-anchor="middle">${t}</text>`);
    }
    parts.push(`<text x="0" y="${axisH + laneH / 2 + 4}" font-size="11" font-weight="600" fill="#14161a">CPU</text>`);
  }

  // CPU lane
  const laneY = axisH;
  parts.push(`<rect x="${labelW}" y="${laneY}" width="${cols * cell}" height="${laneH}" rx="4" fill="#f5f4ef" stroke="#dddbd2"/>`);
  for (const seg of run.segments) {
    for (let t = seg.start; t < seg.end; t++) {
      const o = opacityAt(t);
      if (!o) continue;
      parts.push(`<rect x="${x(t) + 0.5}" y="${laneY + 1}" width="${cell - 1}" height="${laneH - 2}" fill="${colorOf(seg.p)}" opacity="${o}"/>`);
    }
    const visibleEnd = ghost ? seg.end : Math.min(seg.end, reveal);
    if (!mini && visibleEnd > seg.start && (visibleEnd - seg.start) * cell >= 14) {
      const label = esc(processes[seg.p]?.name ?? '');
      parts.push(`<text x="${(x(seg.start) + x(visibleEnd)) / 2}" y="${laneY + laneH / 2 + 4}" font-size="12" font-weight="600" fill="#fff" text-anchor="middle" opacity="${seg.start < reveal ? 1 : 0.4}">${label}</text>`);
    }
  }

  // Process rows
  for (let p = 0; p < rows; p++) {
    const y = axisH + laneH + 10 + p * rowH;
    const proc = processes[p];
    const line = run.timeline[p] ?? '';
    parts.push(`<text x="0" y="${y + 15}" font-size="11.5" fill="#14161a"><tspan fill="${colorOf(p)}">■</tspan> ${esc(proc.name)}</text>`);
    parts.push(`<line x1="${labelW}" x2="${x(cols)}" y1="${y + rowH - 1}" y2="${y + rowH - 1}" stroke="#ecebe4"/>`);
    for (let t = 0; t < cols; t++) {
      const o = opacityAt(t);
      if (!o) continue;
      const c = line[t];
      if (c === '*') parts.push(`<rect x="${x(t) + 0.5}" y="${y + 3}" width="${cell - 1}" height="${rowH - 7}" rx="2" fill="${colorOf(p)}" opacity="${o}"/>`);
      else if (c === '.') parts.push(`<rect x="${x(t) + 0.5}" y="${y + 3}" width="${cell - 1}" height="${rowH - 7}" fill="url(#${hatch})" stroke="#c9c6bb" stroke-width="0.6" opacity="${o}"/>`);
    }
    if (proc.arrival < cols) parts.push(`<line x1="${x(proc.arrival)}" x2="${x(proc.arrival)}" y1="${y + 1}" y2="${y + rowH - 3}" stroke="#14161a" stroke-width="2"/>`);
  }

  // Playhead
  if (Number.isFinite(upto) && !mini) {
    const px = x(Math.min(upto, cols));
    parts.push(`<line x1="${px}" x2="${px}" y1="${axisH - 4}" y2="${height - 2}" stroke="#0f9f78" stroke-width="2"/>`);
    parts.push(`<circle cx="${px}" cy="${axisH - 4}" r="4" fill="#0f9f78"/>`);
  }

  parts.push('</svg>');
  host.innerHTML = parts.join('');
}
