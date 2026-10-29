// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstdint>

// Avoid overflow when converting a device-memory byte count to a percent.
inline int GpuMemoryPercent(std::uint64_t used, std::uint64_t total)
{
    if (total == 0 || used > total)
        return -1;

    return static_cast<int>(100.0 * static_cast<double>(used) /
                            static_cast<double>(total) + 0.5);
}
