// Categorical palette chosen for distinguishability on white and for colour-blind users.
export const COLORS = ['#2f6fdf', '#e0702b', '#0f9f78', '#c23d78', '#7a5ae0', '#b88a00', '#1b93b8', '#8b6648', '#d14545', '#4d7c0f'];
export const colorOf = (index) => COLORS[index % COLORS.length];

const ESCAPES = { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' };
export const esc = (value) => String(value).replace(/[&<>"']/g, (c) => ESCAPES[c]);

export const fmt = (n, digits = 2) => (Number.isFinite(n) ? n.toFixed(digits) : '—');
export const pct = (x, digits = 1) => `${x > 0 ? '−' : x < 0 ? '+' : ''}${Math.abs(x * 100).toFixed(digits)}%`;

export const pad2 = (n) => String(n).padStart(2, '0');

export const reducedMotion = () => window.matchMedia('(prefers-reduced-motion: reduce)').matches;

let svgCounter = 0;
export const uid = (prefix) => `${prefix}-${++svgCounter}`;
