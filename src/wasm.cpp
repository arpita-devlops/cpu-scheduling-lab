// WebAssembly bindings: the exact same C++ engine powers the CLI and the browser visualizer.
#include <emscripten/bind.h>

#include <sstream>
#include <string>

#include "scheduler/engine.hpp"

namespace {

std::string errorJson(const std::exception& e) { return "{\"error\":" + sched::jsonString(e.what()) + "}"; }

std::string simulate(const std::string& input) {
    try {
        std::istringstream in(input);
        const sched::Workload workload = sched::parseWorkload(in);
        return sched::toJson(workload, sched::simulateAll(workload));
    } catch (const std::exception& e) {
        return errorJson(e);
    }
}

std::string legacyReport(const std::string& input) {
    try {
        std::istringstream in(input);
        return sched::formatLegacyReport(sched::parseWorkload(in));
    } catch (const std::exception& e) {
        return std::string("error: ") + e.what();
    }
}

std::string benchmark(int runs, unsigned seed, const std::string& profile) {
    try {
        return sched::benchmarkJson(runs, seed, profile);
    } catch (const std::exception& e) {
        return errorJson(e);
    }
}

}  // namespace

EMSCRIPTEN_BINDINGS(scheduler) {
    emscripten::function("simulate", &simulate);
    emscripten::function("legacyReport", &legacyReport);
    emscripten::function("benchmark", &benchmark);
    emscripten::function("catalog", &sched::catalogJson);
}
