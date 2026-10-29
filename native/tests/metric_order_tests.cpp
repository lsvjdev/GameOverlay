#include "../metric_order.h"
#include <cassert>

int main()
{
    auto order = DefaultMetricOrder;
    assert(IsValidMetricOrder(order));
    assert(MoveMetric(order, 1, -1));
    assert(order[0] == MetricTime && order[1] == MetricFps);
    assert(MoveMetric(order, 0, 1));
    assert(order == DefaultMetricOrder);
    assert(!MoveMetric(order, 0, -1));
    assert(!MoveMetric(order, 5, 1));
    order[1] = order[0];
    assert(!IsValidMetricOrder(order));
}
