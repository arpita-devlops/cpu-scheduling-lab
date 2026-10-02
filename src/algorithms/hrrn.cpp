#include <algorithm>
#include <tuple>

#include "common.hpp"

namespace sched {
namespace {

class HighestResponseRatioNext final : public Scheduler {
public:
    AlgorithmInfo info() const override {
        return {"5", "HRRN", "Highest Response Ratio Next", "Non-preemptive", false, "", -1,
                "Run the process with the highest (waiting + service) / service ratio.",
                "Favours short jobs like SPN, but a job's ratio grows the longer it waits, so long jobs "
                "eventually win — built-in protection against starvation."};
    }

    void schedule(SimulationContext& ctx, int /*param*/) const override {
        const auto& order = ctx.arrivalOrder();
        std::vector<std::tuple<int, double, int>> ready;  // (process, response ratio, ticks served)
        size_t next = 0;
        for (int t = 0; t < ctx.horizon(); ++t) {
            while (next < order.size() && ctx.process(order[next]).arrival <= t) {
                ready.emplace_back(order[next], 1.0, 0);
                ++next;
            }
            for (auto& entry : ready) {
                const Process& proc = ctx.process(std::get<0>(entry));
                std::get<1>(entry) = (t - proc.arrival + proc.service) * 1.0 / proc.service;
            }
            std::stable_sort(ready.begin(), ready.end(),
                             [](const auto& a, const auto& b) { return std::get<1>(a) > std::get<1>(b); });
            if (ready.empty()) continue;

            std::vector<std::pair<int, std::string>> ranked;
            for (const auto& entry : ready) ranked.emplace_back(std::get<0>(entry), detail::fixed2(std::get<1>(entry)));
            const int p = std::get<0>(ready.front());
            ctx.decide(t, p, "Highest response ratio (wait + service) / service: " + detail::compare(ctx, ranked));

            while (t < ctx.horizon() && std::get<2>(ready.front()) != ctx.process(p).service) {
                ctx.run(t, p);
                ++t;
                ++std::get<2>(ready.front());
            }
            --t;
            ready.erase(ready.begin());
            ctx.complete(p, t + 1);
        }
        ctx.fillWaiting();
    }
};

}  // namespace

std::unique_ptr<Scheduler> makeHighestResponseRatioNext() { return std::make_unique<HighestResponseRatioNext>(); }

}  // namespace sched
