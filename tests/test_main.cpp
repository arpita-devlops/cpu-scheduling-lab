// Dependency-free test runner: regression tests against the original lab outputs + unit tests.
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "scheduler/engine.hpp"
#include "scheduler/scheduler.hpp"

namespace fs = std::filesystem;
using namespace sched;

namespace {

int failures = 0;
int checks = 0;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        ++checks;                                                                      \
        if (!(cond)) {                                                                 \
            ++failures;                                                                \
            std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                              \
    } while (0)

#define CHECK_NEAR(a, b) CHECK(std::abs((a) - (b)) < 1e-6)

Workload workload(const std::string& text) {
    std::istringstream in(text);
    return parseWorkload(in);
}

ScheduleResult run(const std::string& text) {
    const Workload w = workload(text);
    return simulate(w.processes, w.algorithms.front(), w.horizon);
}

std::string readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    std::string s = ss.str();
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
    return s;
}

// One original expected file lacks the final blank line, so trailing newlines are not significant.
std::string trimEnd(std::string s) {
    while (!s.empty() && s.back() == '\n') s.pop_back();
    return s;
}

void regression(const fs::path& dir) {
    std::vector<fs::path> inputs;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.path().filename().string().find("-input.txt") != std::string::npos) inputs.push_back(entry.path());
    }
    std::sort(inputs.begin(), inputs.end());
    for (const auto& input : inputs) {
        std::string expectedName = input.filename().string();
        expectedName.replace(expectedName.find("-input"), 6, "-output");
        const std::string expected = readFile(input.parent_path() / expectedName);
        std::istringstream in(readFile(input));
        const std::string actual = formatLegacyReport(parseWorkload(in));
        ++checks;
        if (trimEnd(actual) != trimEnd(expected)) {
            ++failures;
            std::cerr << "  FAIL regression " << input.filename().string() << "\n--- expected\n" << expected << "--- actual\n" << actual;
        }
    }
    std::cout << "  regression: " << inputs.size() << " original lab test cases\n";
}

void unitTests() {
    // FCFS metrics on a gap-free workload: A(0,3) B(1,2) C(2,1)
    {
        const auto r = run("json\n1\n10\n3\nA,0,3\nB,1,2\nC,2,1\n");
        CHECK(r.finish == (std::vector<int>{3, 5, 6}));
        CHECK(r.metrics[1].waiting == 2 && r.metrics[2].waiting == 3);
        CHECK(r.metrics[2].response == 3);
        CHECK_NEAR(r.summary.avgWaiting, 5.0 / 3);
        CHECK_NEAR(r.summary.cpuUtilization, 1.0);
        CHECK(r.summary.contextSwitches == 2);
        CHECK(r.completes);
    }
    // FCFS waits for late arrivals instead of running them early (idle CPU is counted)
    {
        const auto r = run("json\n1\n12\n2\nA,0,2\nB,5,3\n");
        CHECK(r.finish == (std::vector<int>{2, 8}));
        CHECK(r.timeline[4][1] == kIdle && r.timeline[5][1] == kRunning);
        CHECK(r.summary.idleTicks == 3);
        CHECK_NEAR(r.summary.cpuUtilization, 5.0 / 8);
    }
    // Round Robin alternates every quantum
    {
        const auto r = run("json\n2-1\n6\n2\nA,0,2\nB,0,2\n");
        CHECK(r.timeline[0][0] == kRunning && r.timeline[1][1] == kRunning && r.timeline[2][0] == kRunning);
        CHECK(r.summary.contextSwitches == 3);
    }
    // SRT preempts a long job when a shorter one arrives
    {
        const auto r = run("json\n4\n12\n2\nA,0,8\nB,2,2\n");
        CHECK(r.finish[1] == 4);
        CHECK(r.metrics[1].response == 0);
    }
    // Priority: higher value runs first; preemptive version interrupts on arrival
    {
        const auto np = run("json\n9\n12\n3\nA,0,3,1\nB,1,2,5\nC,1,2,3\n");
        CHECK(np.finish == (std::vector<int>{3, 5, 7}));
        const auto p = run("json\n10\n12\n2\nA,0,4,1\nB,1,2,9\n");
        CHECK(p.finish == (std::vector<int>{6, 3}));
        CHECK(!p.decisions.empty() && p.decisions[1].reason.find("Preempts A") != std::string::npos);
    }
    // Aging models a long-running system: no completions, every process eventually runs
    {
        const auto r = run("json\n8-1\n20\n2\nA,0,9\nB,0,1\n");
        CHECK(!r.completes);
        bool bRan = false;
        for (const auto& tick : r.timeline) bRan |= tick[1] == kRunning;
        CHECK(bRan);
    }
    // Every dispatch records a human-readable greedy justification
    {
        const auto r = run("json\n5\n20\n3\nA,0,3\nB,1,6\nC,2,2\n");
        CHECK(r.decisions.size() == 3);
        for (const auto& d : r.decisions) CHECK(!d.reason.empty());
    }
    // Registry + parser validation
    {
        CHECK(AlgorithmRegistry::instance().ids().size() == 10);
        bool threw = false;
        try {
            workload("trace\n42\n10\n1\nA,0,1\n");
        } catch (const std::exception&) {
            threw = true;
        }
        CHECK(threw);
        threw = false;
        try {
            workload("trace\n1\n10\n1\nA,0,0\n");
        } catch (const std::exception&) {
            threw = true;
        }
        CHECK(threw);
    }
    // Benchmarks are reproducible and show the greedy shortest-job advantage on convoy workloads
    {
        CHECK(benchmarkJson(20, 7, "mixed") == benchmarkJson(20, 7, "mixed"));
        const Workload w = randomWorkload(3, "convoy");
        const auto fcfs = simulate(w.processes, {"1", -1}, w.horizon);
        const auto srt = simulate(w.processes, {"4", -1}, w.horizon);
        CHECK(srt.summary.avgWaiting < fcfs.summary.avgWaiting);
    }
    // JSON output is well-formed enough to contain every run
    {
        const Workload w = workload("json\n1,2-2,3\n20\n2\nA,0,3\nB,1,2\n");
        const std::string json = toJson(w, simulateAll(w));
        CHECK(json.find("\"label\":\"RR-2\"") != std::string::npos);
        CHECK(json.find("\"segments\"") != std::string::npos);
    }
}

}  // namespace

int main(int argc, char** argv) {
    const fs::path dir = argc > 1 ? fs::path(argv[1]) : fs::path("testcases");
    std::cout << "Running scheduler tests\n";
    regression(dir);
    unitTests();
    std::cout << "  " << (checks - failures) << "/" << checks << " checks passed\n";
    return failures ? 1 : 0;
}
