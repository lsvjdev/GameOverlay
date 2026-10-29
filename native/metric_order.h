// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <utility>

enum OverlayMetric : unsigned
{
    MetricFps = 1u << 0,
    MetricTime = 1u << 1,
    MetricCpu = 1u << 2,
    MetricGpu = 1u << 3,
    MetricVram = 1u << 4,
    MetricRam = 1u << 5
};

inline constexpr std::array<unsigned, 6> DefaultMetricOrder{
    MetricFps, MetricTime, MetricCpu, MetricGpu, MetricVram, MetricRam
};

inline bool IsValidMetricOrder(const std::array<unsigned, 6>& order)
{
    unsigned seen = 0;
    for (unsigned metric : order)
    {
        if (metric == 0 || (metric & (metric - 1)) != 0 ||
            metric > MetricRam || (seen & metric))
            return false;
        seen |= metric;
    }
    return seen == 63;
}

inline bool MoveMetric(std::array<unsigned, 6>& order, int index, int direction)
{
    const int next = index + direction;
    if (index < 0 || index >= 6 || next < 0 || next >= 6)
        return false;

    std::swap(order[index], order[next]);
    return true;
}
