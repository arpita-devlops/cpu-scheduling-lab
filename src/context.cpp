#include "scheduler/context.hpp"

#include <algorithm>
#include <numeric>
#include <utility>

namespace sched {

SimulationContext::SimulationContext(std::vector<Process> processes, int horizon)
    : processes_(std::move(processes)),
      horizon_(horizon),
      timeline_(static_cast<size_t>(std::max(horizon, 0)), std::vector<char>(processes_.size(), kIdle)),
      finish_(processes_.size(), 0) {
    arrivalOrder_.resize(processes_.size());
    std::iota(arrivalOrder_.begin(), arrivalOrder_.end(), 0);
    std::stable_sort(arrivalOrder_.begin(), arrivalOrder_.end(),
                     [this](int a, int b) { return processes_[a].arrival < processes_[b].arrival; });
}

void SimulationContext::run(int tick, int process) {
    if (tick >= 0 && tick < horizon_) timeline_[tick][process] = kRunning;
}

void SimulationContext::complete(int process, int finishTime) {
    finish_[process] = finishTime;
}

void SimulationContext::decide(int tick, int process, std::string reason) {
    if (!decisions_.empty() && decisions_.back().process == process) return;
    decisions_.push_back({tick, process, std::move(reason)});
}

void SimulationContext::fillWaiting() {
    for (int p = 0; p < count(); ++p) {
        // Processes that never finish (e.g. Aging) wait until the end of the horizon.
        const int end = finish_[p] > 0 ? std::min(finish_[p], horizon_) : horizon_;
        for (int t = std::max(processes_[p].arrival, 0); t < end; ++t) {
            if (timeline_[t][p] != kRunning) timeline_[t][p] = kWaiting;
        }
    }
}

ScheduleResult SimulationContext::finish() && {
    ScheduleResult result;
    result.timeline = std::move(timeline_);
    result.finish = std::move(finish_);
    result.decisions = std::move(decisions_);
    return result;
}

}  // namespace sched
