#pragma once
#include <stdint.h>

struct TracePoint
{
    uint64_t id;
    uint64_t start;
    uint64_t end;
};