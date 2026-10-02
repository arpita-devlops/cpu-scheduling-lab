#include <algorithm>

#include "common.hpp"

namespace sched {
namespace {

class FirstComeFirstServed final : public Scheduler {
public:
    AlgorithmInfo info() const override {
        return {"1", "FCFS", "First Come, First Served", "Non-preemptive", false, "", -1,
                "Run whichever process arrived first.",
                "Processes run to completion in arrival order. Simple and predictable, but one long job at the front "
                "makes everyone behind it wait (the convoy effect)."};
    }

    void schedule(SimulationContext& ctx, int /*param*/) const override {
        if (ctx.count() == 0) return;
        int time = ctx.process(ctx.arrivalOrder().front()).arrival;
        for (const int p : ctx.arrivalOrder()) {
            const Process& proc = ctx.process(p);
            time = std::max(time, proc.arrival);  // CPU idles until the next arrival
            ctx.decide(time, p, "Earliest arrival still waiting (arrived at t=" + std::to_string(proc.arrival) + ")");
            for (int t = time; t < time + proc.service; ++t) ctx.run(t, p);
            time += proc.service;
            ctx.complete(p, time);
        }
        ctx.fillWaiting();
    }
};

}  // namespace

std::unique_ptr<Scheduler> makeFcfs() { return std::make_unique<FirstComeFirstServed>(); }

}  // namespace sched
