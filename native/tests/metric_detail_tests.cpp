// SPDX-License-Identifier: GPL-3.0-only
#include "../settings_config.h"
#include <cassert>

int main()
{
    AppSettings settings;
    assert(settings.HasDetail(MetricCpu, 1));
    assert(settings.HasDetail(MetricCpu, 2));
    assert(settings.HasDetail(MetricCpu, 4));
    assert(!settings.HasDetail(MetricGpu, 4));
    assert(!settings.HasDetail(MetricVram, 2));

    assert(settings.ToggleDetail(MetricCpu, 4));
    assert(!settings.HasDetail(MetricCpu, 4));
    assert(settings.ToggleDetail(MetricCpu, 2));
    assert(!settings.HasDetail(MetricCpu, 2));
    assert(!settings.ToggleDetail(MetricCpu, 1)); // Cannot hide final sub-reading.
    assert(settings.HasDetail(MetricCpu, 1));
    assert(settings.ToggleDetail(MetricCpu, 4));
    assert(settings.HasDetail(MetricCpu, 4));

    assert(settings.ToggleDetail(MetricVram, 2));
    assert(settings.HasDetail(MetricVram, 2));
    assert(settings.ToggleDetail(MetricVram, 1));
    assert(!settings.HasDetail(MetricVram, 1));
    assert(!settings.ToggleDetail(MetricVram, 2));
    assert(settings.HasDetail(MetricVram, 2));

    assert(!settings.ToggleDetail(MetricFps, 2));
    assert(!settings.ToggleDetail(0, 1));
    assert(settings.MetricIndex(MetricCpu) == 2);
    assert(settings.MetricIndex(MetricRam) == 5);
    return 0;
}
