#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <sstream>

#include "scheduler/engine.hpp"
#include "scheduler/scheduler.hpp"

namespace sched {
namespace {

std::string num(double value) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.4f", value);
    std::string s = buffer;
    s.erase(s.find_last_not_of('0') + 1);
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}

std::string label(const AlgorithmSpec& spec) {
    return AlgorithmRegistry::instance().get(spec.id).statsLabel(spec.param);
}

// Small portable PRNG so benchmarks are identical in the CLI, the browser and CI.
struct Rng {
    unsigned state;
    explicit Rng(unsigned seed) : state(seed ? seed : 0x9E3779B9u) {}
    unsigned next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    int range(int lo, int hi) { return lo + static_cast<int>(next() % static_cast<unsigned>(hi - lo + 1)); }
};

struct Stat {
    std::vector<double> values;
    void add(double v) { values.push_back(v); }
    std::string json() {
        std::sort(values.begin(), values.end());
        double sum = 0;
        for (const double v : values) sum += v;
        const auto pct = [&](double q) { return values[static_cast<size_t>(std::floor(q * (values.size() - 1)))]; };
        return "{\"mean\":" + num(sum / values.size()) + ",\"p50\":" + num(pct(0.5)) + ",\"p95\":" + num(pct(0.95)) +
               ",\"min\":" + num(values.front()) + ",\"max\":" + num(values.back()) + "}";
    }
    double mean() const {
        double sum = 0;
        for (const double v : values) sum += v;
        return values.empty() ? 0 : sum / values.size();
    }
    double max() const { return values.empty() ? 0 : *std::max_element(values.begin(), values.end()); }
};

const std::vector<AlgorithmSpec>& benchmarkSet() {
    // Aging is excluded: it models a long-running system where processes never complete.
    static const std::vector<AlgorithmSpec> specs = {{"1", -1}, {"2", 2}, {"2", 4}, {"3", -1}, {"4", -1},
                                                     {"5", -1}, {"6", -1}, {"7", -1}, {"9", -1}, {"10", -1}};
    return specs;
}

struct BenchmarkData {
    std::vector<AlgorithmSpec> specs;
    std::map<size_t, std::map<std::string, Stat>> stats;
};

BenchmarkData runBenchmark(int runs, unsigned seed, const std::string& profile) {
    BenchmarkData data{benchmarkSet(), {}};
    Rng seeds(seed);
    for (int r = 0; r < runs; ++r) {
        const Workload w = randomWorkload(seeds.next(), profile);
        double fcfsWait = 0;
        for (size_t a = 0; a < data.specs.size(); ++a) {
            const ScheduleResult res = simulate(w.processes, data.specs[a], w.horizon);
            const Summary& s = res.summary;
            if (a == 0) fcfsWait = s.avgWaiting;
            auto& st = data.stats[a];
            st["avgWaiting"].add(s.avgWaiting);
            st["avgTurnaround"].add(s.avgTurnaround);
            st["avgResponse"].add(s.avgResponse);
            st["avgNormTurn"].add(s.avgNormTurn);
            st["cpuUtilization"].add(s.cpuUtilization);
            st["contextSwitches"].add(s.contextSwitches);
            st["fairness"].add(s.fairness);
            st["waitingReduction"].add(fcfsWait > 0 ? (fcfsWait - s.avgWaiting) / fcfsWait : 0);
        }
    }
    return data;
}

}  // namespace

std::string jsonString(const std::string& value) {
    std::string out = "\"";
    for (const char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buffer[8];
                    std::snprintf(buffer, sizeof buffer, "\\u%04x", c);
                    out += buffer;
                } else {
                    out += c;
                }
        }
    }
    return out + "\"";
}

std::string toJson(const Workload& w, const std::vector<RunResult>& runs) {
    std::ostringstream os;
    os << "{\"horizon\":" << w.horizon << ",\"processes\":[";
    for (size_t i = 0; i < w.processes.size(); ++i) {
        const Process& p = w.processes[i];
        os << (i ? "," : "") << "{\"name\":" << jsonString(p.name) << ",\"arrival\":" << p.arrival << ",\"service\":" << p.service
           << ",\"priority\":" << p.effectivePriority() << "}";
    }
    os << "],\"runs\":[";
    for (size_t k = 0; k < runs.size(); ++k) {
        const auto& [spec, r] = runs[k];
        const AlgorithmInfo info = AlgorithmRegistry::instance().get(spec.id).info();
        os << (k ? "," : "") << "{\"id\":" << jsonString(spec.id) << ",\"param\":" << spec.param << ",\"label\":" << jsonString(label(spec))
           << ",\"name\":" << jsonString(info.name) << ",\"completes\":" << (r.completes ? "true" : "false");

        os << ",\"timeline\":[";
        for (size_t p = 0; p < w.processes.size(); ++p) {
            std::string row;
            for (int t = 0; t < w.horizon; ++t) row += r.timeline[t][p];
            os << (p ? "," : "") << jsonString(row);
        }

        os << "],\"segments\":[";
        bool first = true;
        int start = -1, current = -1;
        for (int t = 0; t <= w.horizon; ++t) {
            int running = -1;
            if (t < w.horizon) {
                for (size_t p = 0; p < w.processes.size(); ++p) {
                    if (r.timeline[t][p] == kRunning) running = static_cast<int>(p);
                }
            }
            if (running != current) {
                if (current >= 0) {
                    os << (first ? "" : ",") << "{\"p\":" << current << ",\"start\":" << start << ",\"end\":" << t << "}";
                    first = false;
                }
                current = running;
                start = t;
            }
        }

        os << "],\"decisions\":[";
        for (size_t d = 0; d < r.decisions.size(); ++d) {
            os << (d ? "," : "") << "{\"t\":" << r.decisions[d].time << ",\"p\":" << r.decisions[d].process
               << ",\"reason\":" << jsonString(r.decisions[d].reason) << "}";
        }

        os << "],\"metrics\":[";
        for (size_t p = 0; p < r.metrics.size(); ++p) {
            const ProcessMetrics& m = r.metrics[p];
            os << (p ? "," : "") << "{\"finish\":" << m.finish << ",\"turnaround\":" << m.turnaround << ",\"normTurn\":" << num(m.normTurn)
               << ",\"waiting\":" << m.waiting << ",\"response\":" << m.response << "}";
        }

        const Summary& s = r.summary;
        os << "],\"summary\":{\"avgTurnaround\":" << num(s.avgTurnaround) << ",\"avgNormTurn\":" << num(s.avgNormTurn)
           << ",\"avgWaiting\":" << num(s.avgWaiting) << ",\"avgResponse\":" << num(s.avgResponse)
           << ",\"cpuUtilization\":" << num(s.cpuUtilization) << ",\"throughput\":" << num(s.throughput)
           << ",\"fairness\":" << num(s.fairness) << ",\"makespan\":" << s.makespan << ",\"contextSwitches\":" << s.contextSwitches
           << ",\"idleTicks\":" << s.idleTicks << "}}";
    }
    os << "]}";
    return os.str();
}

std::string catalogJson() {
    const auto& registry = AlgorithmRegistry::instance();
    std::ostringstream os;
    os << "[";
    bool first = true;
    for (const auto& id : registry.ids()) {
        const AlgorithmInfo i = registry.get(id).info();
        os << (first ? "" : ",") << "{\"id\":" << jsonString(i.id) << ",\"short\":" << jsonString(i.shortName)
           << ",\"name\":" << jsonString(i.name) << ",\"family\":" << jsonString(i.family)
           << ",\"preemptive\":" << (i.preemptive ? "true" : "false") << ",\"paramName\":" << jsonString(i.paramName)
           << ",\"paramDefault\":" << i.paramDefault << ",\"greedyRule\":" << jsonString(i.greedyRule)
           << ",\"description\":" << jsonString(i.description) << "}";
        first = false;
    }
    os << "]";
    return os.str();
}

Workload randomWorkload(unsigned seed, const std::string& profile) {
    Rng rng(seed);
    Workload w;
    w.operation = "json";
    const auto add = [&](int arrival, int service) {
        const std::string name(1, static_cast<char>('A' + w.processes.size()));
        w.processes.push_back({name, arrival, service, rng.range(1, 5)});
    };

    if (profile == "convoy") {
        add(0, rng.range(12, 16));
        const int shorts = rng.range(4, 6);
        for (int i = 0; i < shorts; ++i) add(rng.range(1, 6), rng.range(1, 3));
    } else if (profile == "interactive") {
        const int n = rng.range(7, 9);
        for (int i = 0; i < n; ++i) add(rng.range(0, 14), i % 4 == 0 ? rng.range(6, 9) : rng.range(1, 3));
    } else if (profile == "cpu-bound") {
        const int n = rng.range(4, 6);
        for (int i = 0; i < n; ++i) add(rng.range(0, 8), rng.range(6, 14));
    } else {
        const int n = rng.range(5, 8);
        for (int i = 0; i < n; ++i) add(rng.range(0, 12), rng.range(1, 10));
    }

    std::stable_sort(w.processes.begin(), w.processes.end(), [](const Process& a, const Process& b) { return a.arrival < b.arrival; });
    for (size_t i = 0; i < w.processes.size(); ++i) w.processes[i].name = std::string(1, static_cast<char>('A' + i));

    int total = 0, lastArrival = 0;
    for (const auto& p : w.processes) {
        total += p.service;
        lastArrival = std::max(lastArrival, p.arrival);
    }
    w.horizon = lastArrival + total + 1;
    w.algorithms = benchmarkSet();
    return w;
}

std::string benchmarkJson(int runs, unsigned seed, const std::string& profile) {
    runs = std::clamp(runs, 1, 5000);
    BenchmarkData data = runBenchmark(runs, seed, profile);
    std::ostringstream os;
    os << "{\"runs\":" << runs << ",\"seed\":" << seed << ",\"profile\":" << jsonString(profile) << ",\"algorithms\":[";
    for (size_t a = 0; a < data.specs.size(); ++a) {
        auto& st = data.stats[a];
        os << (a ? "," : "") << "{\"id\":" << jsonString(data.specs[a].id) << ",\"param\":" << data.specs[a].param
           << ",\"label\":" << jsonString(label(data.specs[a])) << ",\"metrics\":{";
        bool first = true;
        for (auto& [key, stat] : st) {
            os << (first ? "" : ",") << jsonString(key) << ":" << stat.json();
            first = false;
        }
        os << "}}";
    }
    os << "]}";
    return os.str();
}

std::string benchmarkTable(int runs, unsigned seed, const std::string& profile) {
    runs = std::clamp(runs, 1, 5000);
    BenchmarkData data = runBenchmark(runs, seed, profile);
    std::string out = "Benchmark: " + std::to_string(runs) + " random '" + profile + "' workloads (seed " + std::to_string(seed) + ")\n";
    out += "Algorithm   | Avg wait | Avg TAT | Avg resp | CPU util | Switches | Fairness | Wait vs FCFS (mean / best)\n";
    out += "------------+----------+---------+----------+----------+----------+----------+---------------------------\n";
    for (size_t a = 0; a < data.specs.size(); ++a) {
        auto& st = data.stats[a];
        char line[200];
        std::snprintf(line, sizeof line, "%-11s | %8.2f | %7.2f | %8.2f | %7.1f%% | %8.2f | %8.3f | %+6.1f%% / %+6.1f%%\n",
                      label(data.specs[a]).c_str(), st["avgWaiting"].mean(), st["avgTurnaround"].mean(), st["avgResponse"].mean(),
                      st["cpuUtilization"].mean() * 100, st["contextSwitches"].mean(), st["fairness"].mean(),
                      st["waitingReduction"].mean() * 100, st["waitingReduction"].max() * 100);
        out += line;
    }
    out += "(Wait vs FCFS: positive = less average waiting time than First Come, First Served)\n";
    return out;
}

}  // namespace sched
