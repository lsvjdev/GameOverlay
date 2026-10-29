#pragma once

#include <cmath>
#include <cstddef>
#include <deque>

// Stores approximately the most recent second of frame intervals in memory.
class FrameWindow
{
public:
    bool Add(double intervalMs)
    {
        if (!std::isfinite(intervalMs) || intervalMs <= 0.0 || intervalMs > 1000.0)
            return false;

        intervals_.push_back(intervalMs);
        totalMs_ += intervalMs;

        while (intervals_.size() > 1 && totalMs_ > 1000.0)
            EvictOldest();

        while (intervals_.size() > 1024)
            EvictOldest();

        return true;
    }

    int Fps() const
    {
        if (intervals_.size() < 4 || totalMs_ < 100.0)
            return -1;

        return static_cast<int>(std::lround(1000.0 * intervals_.size() / totalMs_));
    }

    std::size_t Samples() const { return intervals_.size(); }

private:
    void EvictOldest()
    {
        totalMs_ -= intervals_.front();
        intervals_.pop_front();
    }

    std::deque<double> intervals_;
    double totalMs_ = 0.0;
};
