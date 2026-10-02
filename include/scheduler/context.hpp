#pragma once

#include <string>
#include <vector>

#include "scheduler/types.hpp"

namespace sched {

// Shared state every scheduler writes into: the timeline grid, completion times and decisions.
class SimulationContext {
public:
    SimulationContext(std::vector<Process> processes, int horizon);

    const Process& process(int index) const { return processes_[index]; }
    int count() const { return static_cast<int>(processes_.size()); }
    int horizon() const { return horizon_; }

    // Process indices ordered by arrival (stable, so input order breaks ties).
    const std::vector<int>& arrivalOrder() const { return arrivalOrder_; }

    void run(int tick, int process);
    void complete(int process, int finishTime);
    bool isComplete(int process) const { return finish_[process] > 0; }

    // Records a dispatch; consecutive decisions for the same process are merged.
    void decide(int tick, int process, std::string reason);

    // Marks '.' for every tick a process spent in the ready queue (arrival → finish, not running).
    void fillWaiting();

    ScheduleResult finish() &&;

private:
    std::vector<Process> processes_;
    int horizon_;
    std::vector<int> arrivalOrder_;
    std::vector<std::vector<char>> timeline_;
    std::vector<int> finish_;
    std::vector<Decision> decisions_;
};

}  // namespace sched
