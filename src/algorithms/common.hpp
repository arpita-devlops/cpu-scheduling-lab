#pragma once

#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "scheduler/context.hpp"
#include "scheduler/scheduler.hpp"

namespace sched::detail {

inline std::string fixed2(double value) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(2) << value;
    return os.str();
}

// "C=4 · B=5 · D=7" — the candidates the greedy rule compared, best first.
inline std::string compare(const SimulationContext& ctx, const std::vector<std::pair<int, std::string>>& ranked) {
    std::string out;
    const size_t shown = ranked.size() < 4 ? ranked.size() : 4;
    for (size_t i = 0; i < shown; ++i) {
        if (i) out += " \xC2\xB7 ";
        out += ctx.process(ranked[i].first).name + "=" + ranked[i].second;
    }
    if (ranked.size() > shown) out += " \xC2\xB7 \xE2\x80\xA6";
    return out;
}

}  // namespace sched::detail
