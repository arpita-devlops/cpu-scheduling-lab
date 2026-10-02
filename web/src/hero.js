import { renderGantt } from './gantt.js';
import { PRESETS } from './workload.js';
import { esc, pad2, reducedMotion } from './util.js';

const $ = (id) => document.getElementById(id);
const CYCLE = ['SRT', 'RR', 'FCFS', 'HRRN'];

/** Self-playing demo in the hero: the same workload replayed under different greedy rules. */
export function initHero(engine) {
  const processes = PRESETS.textbook.processes;
  const specs = CYCLE.map((s) => engine.catalog.find((a) => a.short === s)).filter(Boolean)
    .map((a) => ({ id: a.id, param: a.paramName ? a.paramDefault : 0 }));
  const runs = engine.simulate(processes, specs).runs;
  const host = $('hero-gantt');
  let index = 0;
  let tick = 0;
  let timer = null;

  function explain(run, t) {
    const decision = run.decisions.filter((d) => d.t <= t).at(-1);
    return decision ? `t=${decision.t}: ${processes[decision.p].name} — ${decision.reason}` : 'CPU idle';
  }

  function draw() {
    const run = runs[index];
    const end = run.summary.makespan;
    $('hero-alg').textContent = run.label;
    $('hero-clock').textContent = `t = ${pad2(Math.min(tick, end))}`;
    renderGantt(host, processes, run, { upto: tick, ghost: false });
    $('hero-why').innerHTML = tick >= end
      ? `<b>${esc(run.label)}</b> finished — average waiting ${run.summary.avgWaiting.toFixed(2)}`
      : esc(explain(run, Math.min(tick, end - 1)));
  }

  function step() {
    const end = runs[index].summary.makespan;
    if (tick < end) {
      tick++;
      draw();
      timer = setTimeout(step, 380);
    } else {
      timer = setTimeout(() => {
        index = (index + 1) % runs.length;
        tick = 0;
        draw();
        step();
      }, 2200);
    }
  }

  if (reducedMotion()) {
    tick = Infinity;
    draw();
  } else {
    draw();
    step();
    // Pause while off-screen to save battery.
    new IntersectionObserver(([entry]) => {
      if (entry.isIntersecting && !timer) step();
      else if (!entry.isIntersecting) {
        clearTimeout(timer);
        timer = null;
      }
    }).observe(host);
  }

  return { relayout: draw };
}
