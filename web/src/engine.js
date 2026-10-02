import createSchedulerModule from './engine/scheduler.mjs';

// Thin wrapper over the C++ engine compiled to WebAssembly (src/wasm.cpp).

let modulePromise = null;

const horizonFor = (processes) =>
  Math.max(0, ...processes.map((p) => p.arrival)) + processes.reduce((sum, p) => sum + p.burst, 0) + 1;

const specOf = ({ id, param }) => (param > 0 ? `${id}-${param}` : id);

function toInput(processes, algorithms) {
  const lines = [
    'json',
    algorithms.map(specOf).join(','),
    String(horizonFor(processes)),
    String(processes.length),
    ...processes.map((p) => [p.name, p.arrival, p.burst, ...(Number.isInteger(p.priority) ? [p.priority] : [])].join(',')),
  ];
  return `${lines.join('\n')}\n`;
}

function parse(json) {
  const data = JSON.parse(json);
  if (data.error) throw new Error(data.error);
  const list = Array.isArray(data.runs) ? data.runs : data.algorithms ?? [];
  for (const r of list) r.id = Number(r.id);
  return data;
}

export async function loadEngine() {
  modulePromise ??= createSchedulerModule();
  const wasm = await modulePromise;
  const catalog = JSON.parse(wasm.catalog()).map((a) => ({ ...a, id: Number(a.id) }));

  return {
    catalog,
    /** Runs one or more algorithms on the same processes. */
    simulate: (processes, algorithms) => parse(wasm.simulate(toInput(processes, algorithms))),
    /** Monte-Carlo comparison over seeded random workloads. */
    benchmark: (runs, seed, profile) => parse(wasm.benchmark(runs, seed, profile)),
  };
}
