#pragma once

#include <string>
#include <vector>

namespace sched {

struct Process {
    std::string name;
    int arrival = 0;
    int service = 1;
    int priority = -1;  // higher value = more important; -1 = not given (legacy: service time is used)

    int effectivePriority() const { return priority >= 0 ? priority : service; }
};

struct AlgorithmSpec {
    std::string id;  // registry key, e.g. "2" for Round Robin
    int param = -1;  // quantum / aging interval; -1 when not supplied
};

// One timeline cell per (tick, process): '*' running, '.' waiting in the ready queue, ' ' not present.
constexpr char kRunning = '*';
constexpr char kWaiting = '.';
constexpr char kIdle = ' ';

// A dispatch decision and the greedy rule that justified it.
struct Decision {
    int time = 0;
    int process = -1;
    std::string reason;
};

struct ProcessMetrics {
    int finish = 0;
    int turnaround = 0;
    float normTurn = 0.0F;
    int waiting = 0;
    int response = -1;
};

struct Summary {
    double avgTurnaround = 0;
    double avgNormTurn = 0;
    double avgWaiting = 0;
    double avgResponse = 0;
    double cpuUtilization = 0;  // busy ticks / (makespan - first arrival)
    double throughput = 0;      // processes per tick
    double fairness = 0;        // Jain's index over normalized turnaround (1 = perfectly fair)
    int makespan = 0;
    int contextSwitches = 0;
    int idleTicks = 0;
};

struct ScheduleResult {
    std::vector<std::vector<char>> timeline;  // [tick][process]
    std::vector<int> finish;                  // 0 when the process never completed
    std::vector<Decision> decisions;
    std::vector<ProcessMetrics> metrics;
    Summary summary;
    bool completes = true;
};

struct AlgorithmInfo {
    std::string id;
    std::string shortName;
    std::string name;
    std::string family;       // e.g. "Non-preemptive", "Preemptive", "Multilevel"
    bool preemptive = false;
    std::string paramName;    // empty when the algorithm takes no parameter
    int paramDefault = -1;
    std::string greedyRule;   // the greedy choice made at every dispatch
    std::string description;
};

}  // namespace sched
