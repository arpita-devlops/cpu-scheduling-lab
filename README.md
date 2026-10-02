# Greedy CPU Scheduling Lab

**Who gets the CPU next?** An interactive lab that answers that question ten different ways, using a modular C++17 scheduling engine that runs natively and in the browser (compiled to WebAssembly).

**Live demo:** https://arpita-devlops.github.io/Greedy-CPU-Scheduling/

Every CPU scheduler is a greedy algorithm: at each tick it picks the "best" ready process by one local rule. That might be the earliest arrival, the shortest job, the highest response ratio or the highest priority. This project shows how that single choice changes waiting time, response time and fairness.

## What it demonstrates

| | |
|---|---|
| **Scheduling engine** | FCFS, Round Robin, SPN (SJF), SRT, HRRN, two Feedback variants, Priority (non-preemptive and preemptive) and Priority with Aging. Each one models OS process execution tick by tick. |
| **Interactive visualization** | You can step through the schedule and watch processes move from *incoming* to the *ready queue*, then the *CPU*, then *done*. A live Gantt chart and a "why" panel give the greedy reason for every dispatch. |
| **Comparative analysis** | Every algorithm runs on the same workload side by side, and the best result in each column is highlighted. |
| **Performance benchmarking** | Waiting, turnaround and response time, CPU utilisation, context switches and Jain fairness are measured over thousands of seeded random workloads. On 1,000 mixed workloads, **SRT cuts average waiting time by ~31% compared with FCFS**. |
| **Modular implementation** | Each algorithm is a separate `Scheduler` class behind one interface, registered in an `std::unordered_map` registry. Round Robin uses `std::queue`, and SJF/SRT/Feedback use `std::priority_queue` heaps. |

## Project layout

```
include/scheduler/   public headers (types, Scheduler interface, engine, context)
src/algorithms/      one file per algorithm family
src/engine.cpp       runs schedulers, computes metrics, Monte-Carlo benchmark
src/report.cpp       legacy text trace/stats output + JSON output
src/wasm.cpp         Embind bindings used by the website
app/main.cpp         command-line tool
tests/               53 checks incl. exact-output regression on testcases/
web/                 the website (vanilla JS + Vite, no framework)
```

## Run it

**Command line (Docker, no local toolchain needed)**

```sh
docker build -t cpu-scheduler .
docker run -i --rm cpu-scheduler < testcases/05a-input.txt   # original trace/stats format
docker run --rm cpu-scheduler --benchmark 1000 --seed 42      # algorithm comparison
docker run --rm cpu-scheduler --list                           # algorithm catalog (JSON)
```

**Command line (local CMake)**

```sh
cmake -B build && cmake --build build
ctest --test-dir build --output-on-failure
./build/scheduler < testcases/05a-input.txt
```

**Website**

```sh
cd web
npm install
npx vite            # http://localhost:5173
```

`web/src/engine/scheduler.mjs` is the engine compiled with Emscripten. If you change the C++ code, rebuild it:

```sh
emcmake cmake -B build-wasm -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm --target scheduler_wasm
cp build-wasm/scheduler.mjs web/src/engine/
```

## Input format

```
trace | stats | json
<algorithms, e.g. 1,2-4,5>      # id[-quantum]
<simulation horizon>
<number of processes>
NAME,arrival,burst[,priority]
```

| id | Algorithm | Greedy rule |
|---|---|---|
| 1 | FCFS | earliest arrival |
| 2-q | Round Robin | front of FIFO queue, run for quantum *q* |
| 3 | SPN | shortest burst (non-preemptive) |
| 4 | SRT | shortest remaining time (preemptive) |
| 5 | HRRN | highest (wait + burst) / burst |
| 6 | FB-1 | multilevel feedback, quantum 1 |
| 7 | FB-2i | multilevel feedback, quantum 2^i |
| 8-q | Aging | highest effective priority; waiting raises priority |
| 9 | PRI | highest priority (non-preemptive) |
| 10 | PRI-P | highest priority (preemptive) |

## Deployment

`.github/workflows/pages.yml` runs on every push. It:

1. Builds and tests the engine under AddressSanitizer and UBSan.
2. Compiles the engine to WebAssembly.
3. Builds the site and publishes it to GitHub Pages.

To enable publishing, go to **Settings → Pages → Source** and choose **GitHub Actions**.
