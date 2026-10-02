#include <queue>

#include "common.hpp"

namespace sched {
namespace {

class RoundRobin final : public Scheduler {
public:
    AlgorithmInfo info() const override {
        return {"2", "RR", "Round Robin", "Preemptive", true, "Quantum", 2,
                "Give the CPU to the front of a FIFO queue for one time slice.",
                "Every ready process gets an equal time slice in turn, then goes to the back of the queue. "
                "Great response time and no starvation; the quantum trades responsiveness against switching overhead."};
    }

    std::string statsLabel(int param) const override { return "RR-" + std::to_string(param); }
    std::string traceLabel(int param) const override { return statsLabel(param) + "  "; }

    void schedule(SimulationContext& ctx, int quantum) const override {
        if (quantum <= 0) quantum = 1;
        const auto& order = ctx.arrivalOrder();
        std::queue<std::pair<int, int>> ready;  // (process, remaining service)
        size_t next = 0;
        const auto admit = [&](int tick) {
            while (next < order.size() && ctx.process(order[next]).arrival == tick) {
                ready.emplace(order[next], ctx.process(order[next]).service);
                ++next;
            }
        };

        admit(0);
        int slice = quantum;
        std::string note;
        for (int time = 0; time < ctx.horizon(); ++time) {
            if (ready.empty()) {
                admit(time + 1);
                continue;
            }
            const int p = ready.front().first;
            const int remaining = --ready.front().second;
            if (slice == quantum) {
                ctx.decide(time, p, note + "Front of the FIFO ready queue \xE2\x86\x92 runs for up to " + std::to_string(quantum) + " tick(s)");
                note.clear();
            }
            --slice;
            ctx.run(time, p);
            admit(time + 1);  // new arrivals queue up before the preempted process

            if (remaining == 0) {
                ctx.complete(p, time + 1);
                ready.pop();
                slice = quantum;
            } else if (slice == 0) {
                ready.pop();
                ready.emplace(p, remaining);
                note = "Quantum expired for " + ctx.process(p).name + " (moved to the back). ";
                slice = quantum;
            }
        }
        ctx.fillWaiting();
    }
};

}  // namespace

std::unique_ptr<Scheduler> makeRoundRobin() { return std::make_unique<RoundRobin>(); }

}  // namespace sched
