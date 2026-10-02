#include "scheduler/engine.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <stdexcept>

#include "scheduler/context.hpp"
#include "scheduler/scheduler.hpp"

namespace sched {
namespace {

std::vector<std::string> split(const std::string& text, char sep) {
    std::vector<std::string> parts;
    std::stringstream stream(text);
    std::string item;
    while (std::getline(stream, item, sep)) parts.push_back(item);
    return parts;
}

int toInt(const std::string& text, const std::string& what) {
    try {
        size_t used = 0;
        const int value = std::stoi(text, &used);
        if (used != text.size()) throw std::invalid_argument(text);
        return value;
    } catch (const std::exception&) {
        throw std::runtime_error("Invalid " + what + ": '" + text + "'");
    }
}

std::string printfStr(const char* format, double value) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, format, value);
    return buffer;
}

std::string printfInt(const char* format, int value) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, format, value);
    return buffer;
}

}  // namespace

Workload parseWorkload(std::istream& in) {
    Workload w;
    std::string algorithms;
    int count = 0;
    if (!(in >> w.operation >> algorithms >> w.horizon >> count)) {
        throw std::runtime_error("Expected: <operation> <algorithms> <horizon> <process count>");
    }
    if (w.horizon <= 0 || w.horizon > 100000) throw std::runtime_error("Horizon must be between 1 and 100000");
    if (count <= 0 || count > 1000) throw std::runtime_error("Process count must be between 1 and 1000");

    const auto& registry = AlgorithmRegistry::instance();
    for (const auto& token : split(algorithms, ',')) {
        const auto parts = split(token, '-');
        if (parts.empty() || !registry.contains(parts[0])) throw std::runtime_error("Unknown algorithm '" + token + "'");
        AlgorithmSpec spec{parts[0], parts.size() > 1 ? toInt(parts[1], "parameter") : -1};
        const AlgorithmInfo info = registry.get(spec.id).info();
        if (spec.param <= 0 && info.paramDefault > 0) spec.param = info.paramDefault;
        w.algorithms.push_back(spec);
    }

    for (int i = 0; i < count; ++i) {
        std::string chunk;
        if (!(in >> chunk)) throw std::runtime_error("Expected " + std::to_string(count) + " processes, got " + std::to_string(i));
        const auto fields = split(chunk, ',');
        if (fields.size() < 3 || fields.size() > 4) throw std::runtime_error("Process must be NAME,arrival,service[,priority]: '" + chunk + "'");
        Process p{fields[0], toInt(fields[1], "arrival time"), toInt(fields[2], "service time"),
                  fields.size() == 4 ? toInt(fields[3], "priority") : -1};
        if (p.name.empty() || p.name.size() > 12) throw std::runtime_error("Process names must be 1-12 characters");
        if (p.arrival < 0 || p.service <= 0) throw std::runtime_error("Process " + p.name + " needs arrival >= 0 and service > 0");
        w.processes.push_back(p);
    }
    return w;
}

void computeMetrics(const std::vector<Process>& processes, ScheduleResult& r) {
    const int n = static_cast<int>(processes.size());
    const int horizon = static_cast<int>(r.timeline.size());
    r.metrics.assign(n, {});
    r.completes = true;

    int firstArrival = horizon;
    for (const auto& p : processes) firstArrival = std::min(firstArrival, p.arrival);

    int completed = 0;
    int responded = 0;
    double sumTat = 0, sumNorm = 0, sumWait = 0, sumResp = 0, sumNormSq = 0;
    for (int i = 0; i < n; ++i) {
        ProcessMetrics& m = r.metrics[i];
        m.finish = r.finish[i];
        for (int t = 0; t < horizon; ++t) {
            if (r.timeline[t][i] == kRunning) {
                m.response = t - processes[i].arrival;
                break;
            }
        }
        if (m.finish > 0) {
            m.turnaround = m.finish - processes[i].arrival;
            m.normTurn = static_cast<float>(m.turnaround * 1.0 / processes[i].service);
            m.waiting = m.turnaround - processes[i].service;
            ++completed;
            sumTat += m.turnaround;
            sumNorm += m.normTurn;
            sumNormSq += static_cast<double>(m.normTurn) * m.normTurn;
            sumWait += m.waiting;
        } else {
            r.completes = false;
            for (int t = 0; t < horizon; ++t) m.waiting += r.timeline[t][i] == kWaiting;
        }
        if (m.response >= 0) {
            ++responded;
            sumResp += m.response;
        }
    }

    Summary& s = r.summary;
    int busy = 0, lastBusy = 0, previous = -1;
    for (int t = 0; t < horizon; ++t) {
        int running = -1;
        for (int i = 0; i < n; ++i) {
            if (r.timeline[t][i] == kRunning) running = i;
        }
        if (running < 0) continue;
        ++busy;
        lastBusy = t + 1;
        if (previous >= 0 && running != previous) ++s.contextSwitches;
        previous = running;
    }

    s.makespan = r.completes ? *std::max_element(r.finish.begin(), r.finish.end()) : lastBusy;
    const int span = std::max(s.makespan - firstArrival, 1);
    s.idleTicks = std::max(span - busy, 0);
    s.cpuUtilization = std::min(1.0, busy * 1.0 / span);
    s.throughput = completed * 1.0 / span;
    if (completed) {
        s.avgTurnaround = sumTat / completed;
        s.avgNormTurn = sumNorm / completed;
        s.avgWaiting = sumWait / completed;
        s.fairness = sumNormSq > 0 ? (sumNorm * sumNorm) / (completed * sumNormSq) : 1.0;
    }
    if (responded) s.avgResponse = sumResp / responded;
}

ScheduleResult simulate(const std::vector<Process>& processes, const AlgorithmSpec& spec, int horizon) {
    SimulationContext ctx(processes, horizon);
    AlgorithmRegistry::instance().get(spec.id).schedule(ctx, spec.param);
    ScheduleResult result = std::move(ctx).finish();
    computeMetrics(processes, result);
    return result;
}

std::vector<RunResult> simulateAll(const Workload& workload) {
    std::vector<RunResult> runs;
    runs.reserve(workload.algorithms.size());
    for (const auto& spec : workload.algorithms) runs.emplace_back(spec, simulate(workload.processes, spec, workload.horizon));
    return runs;
}

// ---- Legacy text reports (byte-for-byte compatible with the original lab output) ----

std::string formatTrace(const Workload& w, const RunResult& run) {
    const auto& r = run.second;
    std::string out = AlgorithmRegistry::instance().get(run.first.id).traceLabel(run.first.param);
    for (int i = 0; i <= w.horizon; ++i) out += std::to_string(i % 10) + " ";
    out += "\n------------------------------------------------\n";
    for (size_t p = 0; p < w.processes.size(); ++p) {
        out += w.processes[p].name + "     |";
        for (int t = 0; t < w.horizon; ++t) {
            out += r.timeline[t][p];
            out += '|';
        }
        out += " \n";
    }
    out += "------------------------------------------------\n";
    return out;
}

std::string formatStats(const Workload& w, const RunResult& run) {
    const auto& r = run.second;
    const auto& ps = w.processes;
    std::string out = AlgorithmRegistry::instance().get(run.first.id).statsLabel(run.first.param) + "\n";

    out += "Process    ";
    for (const auto& p : ps) out += "|  " + p.name + "  ";
    out += "|\nArrival    ";
    for (const auto& p : ps) out += printfInt("|%3d  ", p.arrival);
    out += "|\nService    |";
    for (const auto& p : ps) out += printfInt("%3d  |", p.service);
    out += " Mean|\nFinish     ";
    for (const auto& m : r.metrics) out += printfInt("|%3d  ", m.finish);
    out += "|-----|\nTurnaround |";

    int sumTat = 0;
    for (const auto& m : r.metrics) {
        out += printfInt("%3d  |", m.turnaround);
        sumTat += m.turnaround;
    }
    const double meanTat = 1.0 * sumTat / ps.size();
    out += printfStr(meanTat >= 10 ? "%2.2f|\n" : " %2.2f|\n", meanTat);

    out += "NormTurn   |";
    float sumNorm = 0;  // float accumulation matches the original output exactly
    for (const auto& m : r.metrics) {
        out += printfStr(m.normTurn >= 10 ? "%2.2f|" : " %2.2f|", m.normTurn);
        sumNorm += m.normTurn;
    }
    const double meanNorm = 1.0 * sumNorm / ps.size();
    out += printfStr(meanNorm >= 10 ? "%2.2f|\n" : " %2.2f|\n", meanNorm);
    return out;
}

std::string formatLegacyReport(const Workload& w) {
    std::string out;
    for (const auto& run : simulateAll(w)) {
        if (w.operation == "trace") out += formatTrace(w, run);
        else if (w.operation == "stats") out += formatStats(w, run);
        out += "\n";
    }
    return out;
}

}  // namespace sched
