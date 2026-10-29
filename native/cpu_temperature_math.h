// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cmath>
#include <cstdint>

inline int DecodeAmdTemperature(std::uint32_t raw)
{
    if (raw == 0 || raw == 0xffffffffu)
        return -1;

    double celsius = static_cast<double>(raw >> 21) * 0.125;
    if ((raw & 0x80000u) || (raw & 0x30000u) == 0x30000u)
        celsius -= 49.0;

    if (!std::isfinite(celsius) || celsius < 5.0 || celsius > 125.0)
        return -1;

    return static_cast<int>(std::lround(celsius));
}
