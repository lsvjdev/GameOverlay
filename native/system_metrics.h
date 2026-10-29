
#pragma once

#include <windows.h>
#include <cstdint>

struct SystemMetrics
{
    int CpuPercent = -1;
    int RamPercent = -1;

    void Refresh()
    {
        MEMORYSTATUSEX memory{};
        memory.dwLength = sizeof(memory);

        RamPercent = GlobalMemoryStatusEx(&memory)
            ? static_cast<int>(memory.dwMemoryLoad)
            : -1;

        FILETIME idleTime{}, kernelTime{}, userTime{};

        if (!GetSystemTimes(&idleTime, &kernelTime, &userTime))
        {
            CpuPercent = -1;
            _hasPrevious = false;
            return;
        }

        const auto idle = ToTicks(idleTime);
        const auto kernel = ToTicks(kernelTime);
        const auto user = ToTicks(userTime);

        CpuPercent = -1;

        if (_hasPrevious &&
            idle >= _lastIdle &&
            kernel >= _lastKernel &&
            user >= _lastUser)
        {
            const auto idleDelta = idle - _lastIdle;
            const auto totalDelta =
                (kernel - _lastKernel) + (user - _lastUser);

            if (totalDelta > 0 && idleDelta <= totalDelta)
            {
                const double usage =
                    100.0 * static_cast<double>(totalDelta - idleDelta) /
                    static_cast<double>(totalDelta);

                CpuPercent = static_cast<int>(usage + 0.5);
            }
        }

        _lastIdle = idle;
        _lastKernel = kernel;
        _lastUser = user;
        _hasPrevious = true;
    }

private:
    static std::uint64_t ToTicks(const FILETIME& time)
    {
        return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32)
            | time.dwLowDateTime;
    }

    std::uint64_t _lastIdle = 0;
    std::uint64_t _lastKernel = 0;
    std::uint64_t _lastUser = 0;
    bool _hasPrevious = false;
};
