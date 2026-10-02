#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "scheduler/context.hpp"
#include "scheduler/types.hpp"

namespace sched {

// Strategy interface: each scheduling algorithm is one self-contained class.
class Scheduler {
public:
    virtual ~Scheduler() = default;

    virtual AlgorithmInfo info() const = 0;
    virtual void schedule(SimulationContext& ctx, int param) const = 0;

    // Labels used by the legacy text reports ("RR-4", "FB-2i", ...).
    virtual std::string statsLabel(int /*param*/) const { return info().shortName; }
    virtual std::string traceLabel(int param) const;
};

// Hash-map backed registry: adding an algorithm = one new class + one registration line.
class AlgorithmRegistry {
public:
    using Factory = std::function<std::unique_ptr<Scheduler>()>;

    static const AlgorithmRegistry& instance();

    void add(const std::string& id, const Factory& factory);
    bool contains(const std::string& id) const;
    const Scheduler& get(const std::string& id) const;
    std::vector<std::string> ids() const;  // numeric order

private:
    std::unordered_map<std::string, std::unique_ptr<Scheduler>> algorithms_;
};

// Factories implemented in src/algorithms/*.cpp
std::unique_ptr<Scheduler> makeFcfs();
std::unique_ptr<Scheduler> makeRoundRobin();
std::unique_ptr<Scheduler> makeShortestProcessNext();
std::unique_ptr<Scheduler> makeShortestRemainingTime();
std::unique_ptr<Scheduler> makeHighestResponseRatioNext();
std::unique_ptr<Scheduler> makeFeedback();
std::unique_ptr<Scheduler> makeFeedbackExponential();
std::unique_ptr<Scheduler> makeAging();
std::unique_ptr<Scheduler> makePriority();
std::unique_ptr<Scheduler> makePreemptivePriority();

}  // namespace sched
