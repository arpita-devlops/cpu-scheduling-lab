#include <cstring>
#include <iostream>
#include <string>

#include "scheduler/engine.hpp"

namespace {

void usage() {
    std::cout << "Greedy CPU Scheduling Lab\n\n"
                 "Usage:\n"
                 "  scheduler < input.txt                  run the workload (trace / stats / json operation)\n"
                 "  scheduler --json < input.txt           force JSON output (timeline, decisions, metrics)\n"
                 "  scheduler --list                       list algorithms as JSON\n"
                 "  scheduler --benchmark N [--seed S] [--profile mixed|interactive|cpu-bound|convoy] [--json]\n\n"
                 "Input format:\n"
                 "  trace | stats | json\n"
                 "  1,2-4,4          algorithm ids (2-4 = Round Robin, quantum 4)\n"
                 "  20               simulation horizon (ticks)\n"
                 "  3                process count\n"
                 "  A,0,3            NAME,arrival,service[,priority]\n";
}

}  // namespace

int main(int argc, char** argv) {
    bool json = false;
    int benchmark = 0;
    unsigned seed = 42;
    std::string profile = "mixed";

    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            const auto value = [&]() -> std::string {
                if (i + 1 >= argc) throw std::runtime_error("Missing value for " + arg);
                return argv[++i];
            };
            if (arg == "--help" || arg == "-h") {
                usage();
                return 0;
            } else if (arg == "--list") {
                std::cout << sched::catalogJson() << "\n";
                return 0;
            } else if (arg == "--json") {
                json = true;
            } else if (arg == "--benchmark") {
                benchmark = std::stoi(value());
            } else if (arg == "--seed") {
                seed = static_cast<unsigned>(std::stoul(value()));
            } else if (arg == "--profile") {
                profile = value();
            } else {
                throw std::runtime_error("Unknown option " + arg + " (see --help)");
            }
        }

        if (benchmark > 0) {
            std::cout << (json ? sched::benchmarkJson(benchmark, seed, profile) + "\n" : sched::benchmarkTable(benchmark, seed, profile));
            return 0;
        }

        sched::Workload workload = sched::parseWorkload(std::cin);
        if (json || workload.operation == "json") {
            std::cout << sched::toJson(workload, sched::simulateAll(workload)) << "\n";
        } else {
            std::cout << sched::formatLegacyReport(workload);
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
