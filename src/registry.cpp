#include <algorithm>
#include <stdexcept>

#include "scheduler/scheduler.hpp"

namespace sched {

std::string Scheduler::traceLabel(int param) const {
    std::string label = statsLabel(param);
    if (label.size() < 6) label.append(6 - label.size(), ' ');
    else label.append(2, ' ');
    return label;
}

void AlgorithmRegistry::add(const std::string& id, const Factory& factory) {
    algorithms_[id] = factory();
}

bool AlgorithmRegistry::contains(const std::string& id) const {
    return algorithms_.count(id) > 0;
}

const Scheduler& AlgorithmRegistry::get(const std::string& id) const {
    const auto it = algorithms_.find(id);
    if (it == algorithms_.end()) throw std::invalid_argument("Unknown algorithm id: " + id);
    return *it->second;
}

std::vector<std::string> AlgorithmRegistry::ids() const {
    std::vector<std::string> ids;
    ids.reserve(algorithms_.size());
    for (const auto& entry : algorithms_) ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end(), [](const std::string& a, const std::string& b) {
        return a.size() != b.size() ? a.size() < b.size() : a < b;
    });
    return ids;
}

const AlgorithmRegistry& AlgorithmRegistry::instance() {
    static const AlgorithmRegistry registry = [] {
        AlgorithmRegistry r;
        // Ids 1–8 keep the original lab numbering so existing input files still work.
        r.add("1", makeFcfs);
        r.add("2", makeRoundRobin);
        r.add("3", makeShortestProcessNext);
        r.add("4", makeShortestRemainingTime);
        r.add("5", makeHighestResponseRatioNext);
        r.add("6", makeFeedback);
        r.add("7", makeFeedbackExponential);
        r.add("8", makeAging);
        r.add("9", makePriority);
        r.add("10", makePreemptivePriority);
        return r;
    }();
    return registry;
}

}  // namespace sched
