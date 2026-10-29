#include "../gpu_percentage.h"
#include <cassert>
#include <cstdint>
#include <limits>

int main()
{
    constexpr std::uint64_t gib = 1024ULL * 1024 * 1024;
    assert(GpuMemoryPercent(0, 4 * gib) == 0);
    assert(GpuMemoryPercent(2 * gib, 4 * gib) == 50);
    assert(GpuMemoryPercent(4 * gib, 4 * gib) == 100);
    assert(GpuMemoryPercent(1, 0) == -1);
    assert(GpuMemoryPercent(5 * gib, 4 * gib) == -1);
    assert(GpuMemoryPercent(std::numeric_limits<std::uint64_t>::max(),
                            std::numeric_limits<std::uint64_t>::max()) == 100);
}
