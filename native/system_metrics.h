
#pragma once

#include <windows.h>
#include <powrprof.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <cmath>
#include <cstdint>
#include <vector>

struct SystemMetrics
{
    int CpuPercent = -1;
    int CpuFrequencyMhz = -1;
    int RamPercent = -1;

    ~SystemMetrics()
    {
        if (frequencyQuery_)
            PdhCloseQuery(frequencyQuery_);
    }

    void Refresh()
    {
        MEMORYSTATUSEX memory{};
        memory.dwLength = sizeof(memory);

        RamPercent = GlobalMemoryStatusEx(&memory)
            ? static_cast<int>(memory.dwMemoryLoad)
            : -1;

        RefreshFrequency();

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
    // Matches the documented PROCESSOR_POWER_INFORMATION memory layout.
    struct CorePowerInfo
    {
        ULONG number;
        ULONG maxMhz;
        ULONG currentMhz;
        ULONG mhzLimit;
        ULONG maxIdleState;
        ULONG currentIdleState;
    };

    void RefreshFrequency()
    {
        CpuFrequencyMhz = -1;

        if (!frequencyQuery_)
        {
            const DWORD cores = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
            if (cores == 0 || cores > 512)
                return;

            std::vector<CorePowerInfo> coreInfo(cores);
            const LONG status = CallNtPowerInformation(
                ProcessorInformation, nullptr, 0, coreInfo.data(),
                static_cast<ULONG>(coreInfo.size() * sizeof(CorePowerInfo)));

            if (status != 0)
                return;

            std::uint64_t totalRatedMhz = 0;
            unsigned int validCores = 0;

            for (const auto& core : coreInfo)
            {
                if (core.maxMhz >= 100 && core.maxMhz < 15000)
                {
                    totalRatedMhz += core.maxMhz;
                    ++validCores;
                }
            }

            if (!validCores)
                return;

            const int ratedMhz = static_cast<int>(totalRatedMhz / validCores);

            if (PdhOpenQueryW(nullptr, 0, &frequencyQuery_) != ERROR_SUCCESS)
            {
                frequencyQuery_ = nullptr;
                return;
            }

            if (PdhAddEnglishCounterW(
                frequencyQuery_,
                L"\\Processor Information(_Total)\\% Processor Performance",
                0, &performanceCounter_) != ERROR_SUCCESS)
            {
                PdhCloseQuery(frequencyQuery_);
                frequencyQuery_ = nullptr;
                performanceCounter_ = nullptr;
                return;
            }

            ratedFrequencyMhz_ = ratedMhz;
            PdhCollectQueryData(frequencyQuery_);
            return;
        }

        if (PdhCollectQueryData(frequencyQuery_) != ERROR_SUCCESS)
            return;

        PDH_FMT_COUNTERVALUE value{};
        if (PdhGetFormattedCounterValue(
            performanceCounter_, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100,
            nullptr, &value) != ERROR_SUCCESS)
            return;

        if (value.CStatus != PDH_CSTATUS_VALID_DATA &&
            value.CStatus != PDH_CSTATUS_NEW_DATA)
            return;

        const double percent = value.doubleValue;
        if (!std::isfinite(percent) || percent <= 0.0 || percent > 350.0)
            return;

        CpuFrequencyMhz = static_cast<int>(
            std::lround(ratedFrequencyMhz_ * percent / 100.0));
    }

    PDH_HQUERY frequencyQuery_ = nullptr;
    PDH_HCOUNTER performanceCounter_ = nullptr;
    int ratedFrequencyMhz_ = 0;

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
