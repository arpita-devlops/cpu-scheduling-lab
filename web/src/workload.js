// Preset workloads, each chosen to make one scheduling behaviour obvious.
export const PRESETS = {
  textbook: {
    label: 'Textbook example',
    hint: 'The classic five-process example (Stallings) used by the original lab test cases.',
    processes: [
      { name: 'A', arrival: 0, burst: 3 },
      { name: 'B', arrival: 2, burst: 6 },
      { name: 'C', arrival: 4, burst: 4 },
      { name: 'D', arrival: 6, burst: 5 },
      { name: 'E', arrival: 8, burst: 2 },
    ],
  },
  convoy: {
    label: 'Convoy effect',
    hint: 'One long job arrives first. Watch FCFS make every short job wait behind it — then try SRT.',
    processes: [
      { name: 'A', arrival: 0, burst: 12 },
      { name: 'B', arrival: 1, burst: 2 },
      { name: 'C', arrival: 2, burst: 1 },
      { name: 'D', arrival: 3, burst: 2 },
      { name: 'E', arrival: 4, burst: 3 },
    ],
  },
  priority: {
    label: 'Priorities',
    hint: 'Each process has a priority (higher = more important). Compare PRI, PRI-P and AGING.',
    processes: [
      { name: 'A', arrival: 0, burst: 4, priority: 1 },
      { name: 'B', arrival: 1, burst: 3, priority: 3 },
      { name: 'C', arrival: 2, burst: 1, priority: 4 },
      { name: 'D', arrival: 3, burst: 5, priority: 2 },
      { name: 'E', arrival: 4, burst: 2, priority: 5 },
    ],
  },
  starvation: {
    label: 'Starvation',
    hint: 'A low-priority job keeps losing to a stream of important ones. PRI-P starves it; AGING rescues it.',
    processes: [
      { name: 'A', arrival: 0, burst: 5, priority: 1 },
      { name: 'B', arrival: 1, burst: 3, priority: 5 },
      { name: 'C', arrival: 3, burst: 3, priority: 5 },
      { name: 'D', arrival: 5, burst: 3, priority: 5 },
      { name: 'E', arrival: 7, burst: 3, priority: 5 },
      { name: 'F', arrival: 9, burst: 3, priority: 5 },
    ],
  },
  interactive: {
    label: 'Interactive burst',
    hint: 'Many short requests plus one batch job — where Round Robin and Feedback shine on response time.',
    processes: [
      { name: 'A', arrival: 0, burst: 8 },
      { name: 'B', arrival: 1, burst: 1 },
      { name: 'C', arrival: 2, burst: 2 },
      { name: 'D', arrival: 3, burst: 1 },
      { name: 'E', arrival: 4, burst: 2 },
      { name: 'F', arrival: 5, burst: 1 },
    ],
  },
};

export const LIMITS = { maxProcesses: 10, maxArrival: 40, maxBurst: 20, maxPriority: 9 };

export function randomWorkload(count = 3 + Math.floor(Math.random() * 4)) {
  let arrival = 0;
  return Array.from({ length: count }, (_, i) => {
    const proc = {
      name: String.fromCharCode(65 + i),
      arrival,
      burst: 1 + Math.floor(Math.random() * 8),
      priority: 1 + Math.floor(Math.random() * 5),
    };
    arrival += Math.floor(Math.random() * 4);
    return proc;
  });
}

/** Returns an error message, or null when the workload is valid. */
export function validate(processes) {
  if (!processes.length) return 'Add at least one process.';
  if (processes.length > LIMITS.maxProcesses) return `Use at most ${LIMITS.maxProcesses} processes.`;
  const names = new Set();
  for (const p of processes) {
    if (!/^[A-Za-z0-9]{1,4}$/.test(p.name)) return 'Names must be 1–4 letters or digits.';
    if (names.has(p.name)) return `Duplicate name “${p.name}”.`;
    names.add(p.name);
    if (!Number.isInteger(p.arrival) || p.arrival < 0 || p.arrival > LIMITS.maxArrival) return `Arrival must be 0–${LIMITS.maxArrival}.`;
    if (!Number.isInteger(p.burst) || p.burst < 1 || p.burst > LIMITS.maxBurst) return `Burst must be 1–${LIMITS.maxBurst}.`;
    if (p.priority !== undefined && (!Number.isInteger(p.priority) || p.priority < 0 || p.priority > LIMITS.maxPriority)) {
      return `Priority must be 0–${LIMITS.maxPriority} (or empty).`;
    }
  }
  return null;
}
