#pragma once

#include <istream>
#include <string>
#include <utility>
#include <vector>

#include "scheduler/types.hpp"

namespace sched {

struct Workload {
    std::string operation;  // "trace", "stats" or "json"
    std::vector<AlgorithmSpec> algorithms;
    int horizon = 0;
    std::vector<Process> processes;
};

using RunResult = std::pair<AlgorithmSpec, ScheduleResult>;

// Input format (backwards compatible with the original lab):
//   <operation>
//   <algorithms>   e.g. 1,2-4,10-5   (id[-param], comma separated)
//   <horizon>
//   <process count>
//   NAME,arrival,service[,priority]   (one per process)
Workload parseWorkload(std::istream& in);

ScheduleResult simulate(const std::vector<Process>& processes, const AlgorithmSpec& spec, int horizon);
std::vector<RunResult> simulateAll(const Workload& workload);

void computeMetrics(const std::vector<Process>& processes, ScheduleResult& result);

// Reports
std::string formatTrace(const Workload& workload, const RunResult& run);
std::string formatStats(const Workload& workload, const RunResult& run);
std::string formatLegacyReport(const Workload& workload);  // trace/stats exactly like the original CLI
std::string toJson(const Workload& workload, const std::vector<RunResult>& runs);
std::string catalogJson();
std::string jsonString(const std::string& value);

// Monte-Carlo comparison on random workloads. Deterministic for a given seed on every platform.
// Profiles: "mixed", "interactive", "cpu-bound", "convoy".
Workload randomWorkload(unsigned seed, const std::string& profile);
std::string benchmarkJson(int runs, unsigned seed, const std::string& profile);
std::string benchmarkTable(int runs, unsigned seed, const std::string& profile);

}  // namespace sched
