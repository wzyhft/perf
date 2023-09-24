#include "../Perf.h"
#include <thread>

void foo()
{
    static int i = 0;
    while (true)
    {
        PERF_START("FOO");
        while (i < 100)
        {
            ++i;
            usleep(1000);
        }
        PERF_END("FOO");
        i = 0;
        usleep(20000);
    }
}

void bar()
{
    static int i = 0;
    while (true)
    {
        PERF_START("BAR");
        while (i < 200)
        {
            ++i;
            usleep(1000);
        }
        PERF_END("BAR");
        i = 0;
        usleep(20000);
    }
}

int main()
{
    REGISTER_TRACEPOINT("FOO", "BAR");
    auto f = std::thread(foo);
    auto b = std::thread(bar);
    while (true)
    {
        RuntimePerf::poll();
        usleep(1000);
    }
}