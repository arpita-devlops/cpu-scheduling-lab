#include <functional>
#include <queue>

#include "common.hpp"

namespace sched {
namespace {

using MinHeap = std::priority_queue<std::pair<int, int>, std::vector<std::pair<int, int>>, std::greater<>>;

std::vector<std::pair<int, std::string>> ranked(MinHeap heap) {
    std::vector<std::pair<int, std::string>> out;
    while (!heap.empty()) {
        out.emplace_back(heap.top().second, std::to_string(heap.top().first));
        heap.pop();
    }
    return out;
}

class ShortestProcessNext final : public Scheduler {
public:
    AlgorithmInfo info() const override {
        return {"3", "SPN", "Shortest Process Next", "Non-preemptive", false, "", -1,
                "Run the ready process with the smallest total service time.",
                "Also called Shortest Job First. Provably minimises average waiting time when all jobs are known, "
                "but long jobs can starve behind a stream of short ones."};
    }

    void schedule(SimulationContext& ctx, int /*param*/) const override {
        const auto& order = ctx.arrivalOrder();
        MinHeap ready;  // (service, process)
        size_t next = 0;
        for (int t = 0; t < ctx.horizon(); ++t) {
            while (next < order.size() && ctx.process(order[next]).arrival <= t) {
                ready.emplace(ctx.process(order[next]).service, order[next]);
                ++next;
            }
            if (ready.empty()) continue;

            const int p = ready.top().second;
            ctx.decide(t, p, "Shortest service time among ready: " + detail::compare(ctx, ranked(ready)));
            ready.pop();
            const int service = ctx.process(p).service;
            for (int k = t; k < t + service; ++k) ctx.run(k, p);
            ctx.complete(p, t + service);
            t += service - 1;
        }
        ctx.fillWaiting();
    }
};

class ShortestRemainingTime final : public Scheduler {
public:
    AlgorithmInfo info() const override {
        return {"4", "SRT", "Shortest Remaining Time", "Preemptive", true, "", -1,
                "Every tick, run the process with the least work left.",
                "The preemptive version of SPN: a newly arrived short job immediately takes the CPU. "
                "Lowest average waiting time of the classic algorithms, at the cost of more context switches."};
    }

    void schedule(SimulationContext& ctx, int /*param*/) const override {
        const auto& order = ctx.arrivalOrder();
        MinHeap ready;  // (remaining, process)
        size_t next = 0;
        for (int t = 0; t < ctx.horizon(); ++t) {
            while (next < order.size() && ctx.process(order[next]).arrival == t) {
                ready.emplace(ctx.process(order[next]).service, order[next]);
                ++next;
            }
            if (ready.empty()) continue;

            const auto [remaining, p] = ready.top();
            ctx.decide(t, p, "Least remaining work: " + detail::compare(ctx, ranked(ready)));
            ready.pop();
            ctx.run(t, p);
            if (remaining == 1) ctx.complete(p, t + 1);
            else ready.emplace(remaining - 1, p);
        }
        ctx.fillWaiting();
    }
};

}  // namespace

std::unique_ptr<Scheduler> makeShortestProcessNext() { return std::make_unique<ShortestProcessNext>(); }
std::unique_ptr<Scheduler> makeShortestRemainingTime() { return std::make_unique<ShortestRemainingTime>(); }

}  // namespace sched
