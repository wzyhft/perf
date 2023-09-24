#include "RuntimePerf.h"
#include "PerfCommon.h"
#include <unordered_map>

static inline uint64_t rdtsc() { return __builtin_ia32_rdtsc(); }

class Perf
{
public:
    static void startMeasurement(const std::string &name)
    {
        auto t = rdtsc();
        auto id = _contextMap[name];
        _pendingPerfMeasurements.emplace_back(TracePoint{id, t, 0});
    }

    static void endMeasurement(const std::string &name)
    {
        auto t = rdtsc();
        auto &lastMeasurement = _pendingPerfMeasurements.back();
        lastMeasurement.end = t;

        auto header = RuntimePerf::reserveAlloc();
        if (!header)
            return;
        header->id = lastMeasurement.id;
        header->start = lastMeasurement.start;
        header->end = lastMeasurement.end;
        _pendingPerfMeasurements.pop_back();

        RuntimePerf::finishAlloc();
    }

    template <typename T>
    static void registerTracepoint(T name)
    {
        _contextMap[name] = count++;
    }

    template <typename T, typename... Args>
    static void registerTracepoint(T name, Args... names)
    {
        _contextMap[name] = count++;
        registerTracepoint(names...);
    }

    static void printTracepointIdMap()
    {
        for (auto &iter : _contextMap)
        {
            std::cout << iter.first << "->" << iter.second << std::endl;
        }
    }

    static inline thread_local std::vector<TracePoint> _pendingPerfMeasurements;
    static inline std::unordered_map<std::string, uint64_t> _contextMap;
    static inline uint32_t count = 0;
};

#define REGISTER_TRACEPOINT(...)           \
    Perf::registerTracepoint(__VA_ARGS__); \
    Perf::printTracepointIdMap();

#define PERF_START(NAME) Perf::startMeasurement(NAME);
#define PERF_END(NAME) Perf::endMeasurement(NAME);