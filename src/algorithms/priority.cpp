#include <algorithm>
#include <tuple>
#include <unordered_map>

#include "common.hpp"

namespace sched {
namespace {

// Higher priority wins; ties go to the earlier arrival, then input order.
bool morePressing(const SimulationContext& ctx, int a, int b) {
    const Process& pa = ctx.process(a);
    const Process& pb = ctx.process(b);
    if (pa.effectivePriority() != pb.effectivePriority()) return pa.effectivePriority() > pb.effectivePriority();
    if (pa.arrival != pb.arrival) return pa.arrival < pb.arrival;
    return a < b;
}

std::vector<std::pair<int, std::string>> rankByPriority(const SimulationContext& ctx, std::vector<int> ready) {
    std::stable_sort(ready.begin(), ready.end(), [&](int a, int b) { return morePressing(ctx, a, b); });
    std::vector<std::pair<int, std::string>> out;
    for (const int p : ready) out.emplace_back(p, std::to_string(ctx.process(p).effectivePriority()));
    return out;
}

class Priority final : public Scheduler {
public:
    AlgorithmInfo info() const override {
        return {"9", "PRI", "Priority (non-preemptive)", "Non-preemptive", false, "", -1,
                "Run the ready process with the highest priority to completion.",
                "Lets important work go first. Without aging, low-priority processes can starve indefinitely."};
    }
    std::string statsLabel(int /*param*/) const override { return "PRIORITY"; }
    std::string traceLabel(int /*param*/) const override { return "PRI   "; }

    void schedule(SimulationContext& ctx, int /*param*/) const override {
        const auto& order = ctx.arrivalOrder();
        std::vector<int> ready;
        size_t next = 0;
        for (int t = 0; t < ctx.horizon();) {
            while (next < order.size() && ctx.process(order[next]).arrival <= t) ready.push_back(order[next++]);
            if (ready.empty()) {
                t = next < order.size() ? ctx.process(order[next]).arrival : ctx.horizon();
                continue;
            }
            const auto ranked = rankByPriority(ctx, ready);
            const int p = ranked.front().first;
            ctx.decide(t, p, "Highest priority among ready: " + detail::compare(ctx, ranked));
            ready.erase(std::find(ready.begin(), ready.end(), p));
            const int service = ctx.process(p).service;
            for (int k = t; k < t + service; ++k) ctx.run(k, p);
            ctx.complete(p, t + service);
            t += service;
        }
        ctx.fillWaiting();
    }
};

class PreemptivePriority final : public Scheduler {
public:
    AlgorithmInfo info() const override {
        return {"10", "PRI-P", "Priority (preemptive)", "Preemptive", true, "", -1,
                "Every tick, run the highest-priority ready process.",
                "A more important arrival immediately takes the CPU. Highly responsive for urgent work, "
                "but the most exposed to starvation — compare it with Aging."};
    }
    std::string statsLabel(int /*param*/) const override { return "PRIORITY-P"; }
    std::string traceLabel(int /*param*/) const override { return "PRI-P "; }

    void schedule(SimulationContext& ctx, int /*param*/) const override {
        const auto& order = ctx.arrivalOrder();
        std::vector<int> ready;
        std::unordered_map<int, int> remaining;
        size_t next = 0;
        int running = -1;
        for (int t = 0; t < ctx.horizon(); ++t) {
            while (next < order.size() && ctx.process(order[next]).arrival <= t) {
                remaining[order[next]] = ctx.process(order[next]).service;
                ready.push_back(order[next++]);
            }
            if (ready.empty()) continue;

            auto ranked = rankByPriority(ctx, ready);
            int p = ranked.front().first;
            // Keep the running process on ties to avoid needless context switches.
            if (running >= 0 && ctx.process(running).effectivePriority() == ctx.process(p).effectivePriority()) p = running;
            const bool preempts = running >= 0 && running != p && remaining[running] > 0;
            ctx.decide(t, p, (preempts ? "Preempts " + ctx.process(running).name + " \xE2\x80\x94 " : std::string()) +
                                 "highest priority: " + detail::compare(ctx, ranked));

            ctx.run(t, p);
            running = p;
            if (--remaining[p] == 0) {
                ctx.complete(p, t + 1);
                ready.erase(std::find(ready.begin(), ready.end(), p));
                running = -1;
            }
        }
        ctx.fillWaiting();
    }
};

class Aging final : public Scheduler {
public:
    AlgorithmInfo info() const override {
        return {"8", "AGING", "Priority with Aging", "Preemptive", true, "Quantum", 1,
                "Run the highest effective priority; every waiting tick raises a process's priority by 1.",
                "Fixes priority starvation: the longer a process waits, the more important it becomes. "
                "Models a long-running system, so processes never complete within the horizon."};
    }
    std::string traceLabel(int /*param*/) const override { return "Aging "; }

    void schedule(SimulationContext& ctx, int quantum) const override {
        if (quantum <= 0) quantum = 1;
        const auto& order = ctx.arrivalOrder();
        std::vector<std::tuple<int, int, int>> ready;  // (effective priority, process, ticks waited)
        size_t next = 0;
        int current = -1;
        for (int time = 0; time < ctx.horizon(); ++time) {
            while (next < order.size() && ctx.process(order[next]).arrival <= time) {
                ready.emplace_back(ctx.process(order[next]).effectivePriority(), order[next], 0);
                ++next;
            }
            if (ready.empty()) continue;

            for (auto& [prio, p, waited] : ready) {
                if (p == current) {
                    waited = 0;
                    prio = ctx.process(p).effectivePriority();
                } else {
                    ++prio;
                    ++waited;
                }
            }
            std::stable_sort(ready.begin(), ready.end(), [](const auto& a, const auto& b) {
                if (std::get<0>(a) != std::get<0>(b)) return std::get<0>(a) > std::get<0>(b);
                return std::get<2>(a) > std::get<2>(b);
            });

            current = std::get<1>(ready.front());
            std::vector<std::pair<int, std::string>> ranked;
            for (const auto& entry : ready) ranked.emplace_back(std::get<1>(entry), std::to_string(std::get<0>(entry)));
            const int boost = std::get<0>(ready.front()) - ctx.process(current).effectivePriority();
            ctx.decide(time, current, "Highest effective priority (base " + std::to_string(ctx.process(current).effectivePriority()) +
                                          " + " + std::to_string(boost) + " aged): " + detail::compare(ctx, ranked));

            for (int slice = quantum; slice > 0 && time < ctx.horizon(); --slice) ctx.run(time++, current);
            --time;
        }
        ctx.fillWaiting();
    }
};

}  // namespace

std::unique_ptr<Scheduler> makeAging() { return std::make_unique<Aging>(); }
std::unique_ptr<Scheduler> makePriority() { return std::make_unique<Priority>(); }
std::unique_ptr<Scheduler> makePreemptivePriority() { return std::make_unique<PreemptivePriority>(); }

}  // namespace sched
