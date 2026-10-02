#include <functional>
#include <queue>
#include <unordered_map>

#include "common.hpp"

namespace sched {
namespace {

using LevelQueue = std::priority_queue<std::pair<int, int>, std::vector<std::pair<int, int>>, std::greater<>>;

// Multilevel feedback: a process drops one level each time it is preempted while others wait.
class Feedback final : public Scheduler {
public:
    explicit Feedback(bool exponential) : exponential_(exponential) {}

    AlgorithmInfo info() const override {
        if (exponential_) {
            return {"7", "FB-2i", "Feedback (quantum 2\xE2\x81\xB1)", "Multilevel", true, "", -1,
                    "Serve the highest non-empty queue; level i gets a 2^i tick slice.",
                    "Multilevel feedback where lower levels get exponentially longer slices: interactive jobs finish "
                    "fast at the top, CPU-bound jobs sink and run in bigger, cheaper chunks."};
        }
        return {"6", "FB-1", "Feedback (quantum 1)", "Multilevel", true, "", -1,
                "Serve the highest non-empty queue; demote a process after every tick it uses.",
                "Learns job length without being told: new processes start in the top queue and are demoted as they "
                "consume CPU, so short jobs finish quickly."};
    }

    void schedule(SimulationContext& ctx, int /*param*/) const override {
        const auto& order = ctx.arrivalOrder();
        LevelQueue ready;  // (level, process)
        std::unordered_map<int, int> remaining;
        size_t next = 0;
        const auto admit = [&](int tick) {
            while (next < order.size() &&
                   (exponential_ ? ctx.process(order[next]).arrival <= tick : ctx.process(order[next]).arrival == tick)) {
                ready.emplace(0, order[next]);
                remaining[order[next]] = ctx.process(order[next]).service;
                ++next;
            }
        };

        admit(0);
        for (int time = 0; time < ctx.horizon(); ++time) {
            if (!ready.empty()) {
                const auto [level, p] = ready.top();
                ready.pop();
                admit(time + 1);

                int end = time;
                if (exponential_) {
                    int slice = 1 << std::min(level, 30);
                    ctx.decide(time, p, "Q" + std::to_string(level) + " is the highest non-empty queue \xE2\x86\x92 slice of 2^" +
                                            std::to_string(level) + " = " + std::to_string(slice) + " tick(s)");
                    while (slice && remaining[p]) {
                        --slice;
                        --remaining[p];
                        ctx.run(end++, p);
                    }
                } else {
                    ctx.decide(time, p, "Q" + std::to_string(level) + " is the highest non-empty queue (FIFO within the level)");
                    --remaining[p];
                    ctx.run(end++, p);
                }

                if (remaining[p] == 0) ctx.complete(p, end);
                else ready.emplace(ready.empty() ? level : level + 1, p);  // no demotion while running alone
                time = end - 1;
            }
            admit(time + 1);
        }
        ctx.fillWaiting();
    }

private:
    bool exponential_;
};

}  // namespace

std::unique_ptr<Scheduler> makeFeedback() { return std::make_unique<Feedback>(false); }
std::unique_ptr<Scheduler> makeFeedbackExponential() { return std::make_unique<Feedback>(true); }

}  // namespace sched
