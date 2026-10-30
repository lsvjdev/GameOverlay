// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <windows.h>
#include <cstdint>
#include <string>
#include "gpu_percentage.h"

// NVML is loaded from the installed NVIDIA driver.
class GpuMetrics
{
public:
    int UsagePercent = -1;
    int TemperatureC = -1;
    int VramPercent = -1;

    std::uint64_t UsedVramMiB() const { return usedVramMiB_; }
    std::uint64_t TotalVramMiB() const { return totalVramMiB_; }

    ~GpuMetrics()
    {
        Shutdown();
    }

    GpuMetrics(const GpuMetrics&) = delete;
    GpuMetrics& operator=(const GpuMetrics&) = delete;
    GpuMetrics() = default;

    bool Start()
    {
        if (library_)
            return ready_;

        // Never load nvml.dll from the working directory or an untrusted PATH.
        wchar_t systemDirectory[MAX_PATH]{};
        const UINT directoryLength = GetSystemDirectoryW(systemDirectory, MAX_PATH);

        if (directoryLength > 0 && directoryLength < MAX_PATH)
        {
            const std::wstring path =
                std::wstring(systemDirectory) + L"\\nvml.dll";
            library_ = LoadLibraryExW(path.c_str(), nullptr,
                                      LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                                      LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        }

        if (!library_)
        {
            wchar_t programFiles[MAX_PATH]{};
            DWORD length = GetEnvironmentVariableW(
                L"ProgramW6432", programFiles, MAX_PATH);

            if (length > 0 && length < MAX_PATH)
            {
                const std::wstring path = std::wstring(programFiles) +
                    L"\\NVIDIA Corporation\\NVSMI\\nvml.dll";
                library_ = LoadLibraryExW(path.c_str(), nullptr,
                                          LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                                          LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            }
        }

        if (!library_)
        {
            state_ = L"NVIDIA NVML driver library not found";
            win32Error_ = GetLastError();
            return false;
        }

        init_ = Resolve<Init>("nvmlInit_v2");
        shutdown_ = Resolve<ShutdownFn>("nvmlShutdown");
        count_ = Resolve<DeviceCount>("nvmlDeviceGetCount_v2");
        handle_ = Resolve<DeviceHandleByIndex>("nvmlDeviceGetHandleByIndex_v2");
        name_ = Resolve<DeviceName>("nvmlDeviceGetName");
        utilization_ = Resolve<DeviceUtilization>("nvmlDeviceGetUtilizationRates");
        temperature_ = Resolve<DeviceTemperature>("nvmlDeviceGetTemperature");
        memory_ = Resolve<DeviceMemory>("nvmlDeviceGetMemoryInfo");

        if (!init_ || !shutdown_ || !count_ || !handle_)
        {
            state_ = L"NVML is missing required entry points";
            Shutdown();
            return false;
        }

        initResult_ = init_();
        if (initResult_ != 0)
        {
            state_ = L"NVML initialization failed";
            Shutdown();
            return false;
        }

        initialized_ = true;
        unsigned int deviceCount = 0;
        countResult_ = count_(&deviceCount);

        if (countResult_ != 0 || deviceCount == 0)
        {
            state_ = L"No accessible NVIDIA GPU";
            Shutdown();
            return false;
        }

                for (unsigned int i = 0; i < deviceCount && i < 16; ++i)
        {
            NvmlDevice candidate = nullptr;
            if (handle_(i, &candidate) != 0 || !candidate)
                continue;

            device_ = candidate;
            deviceIndex_ = i;

            if (name_)
            {
                char displayName[128]{};
                if (name_(device_, displayName, sizeof(displayName)) == 0)
                    deviceName_ = std::string(displayName);
            }

            ready_ = true;
            state_ = L"NVIDIA NVML ready";
            Refresh();
            return true;
        }

        state_ = L"Unable to access NVIDIA GPU handles";
        Shutdown();
        return false;
    }

    void Refresh()
    {
        UsagePercent = -1;
        TemperatureC = -1;
        VramPercent = -1;

        if (!ready_)
            return;

        if (utilization_)
        {
            NvmlUtilization value{};
            utilizationResult_ = utilization_(device_, &value);
            if (utilizationResult_ == 0 && value.gpu <= 100)
                UsagePercent = static_cast<int>(value.gpu);
        }

        if (temperature_)
        {
            unsigned int value = 0;
            temperatureResult_ = temperature_(device_, 0, &value);
            if (temperatureResult_ == 0 && value > 0 && value < 150)
                TemperatureC = static_cast<int>(value);
        }

        if (memory_)
        {
            NvmlMemory value{};
            memoryResult_ = memory_(device_, &value);
            if (memoryResult_ == 0)
            {
                usedVramMiB_ = value.used / (1024 * 1024);
                totalVramMiB_ = value.total / (1024 * 1024);
                VramPercent = GpuMemoryPercent(value.used, value.total);
            }
        }
    }

    std::wstring Diagnostics() const
    {
        std::wstring report = L"GPU diagnostics\r\nStatus: " + state_;
        report += L"\r\nBackend: NVIDIA NVML (driver-provided)";
        report += L"\r\nDevice index: " + std::to_wstring(deviceIndex_);

        if (!deviceName_.empty())
            report += L"\r\nDevice: " +
                std::wstring(deviceName_.begin(), deviceName_.end());

        report += L"\r\nGPU utilization (%): " + Value(UsagePercent);
        report += L"\r\nGPU temperature (C): " + Value(TemperatureC);
        report += L"\r\nVRAM used/total (MiB): " +
            std::to_wstring(usedVramMiB_) + L"/" +
            std::to_wstring(totalVramMiB_);
        report += L"\r\nVRAM (%): " + Value(VramPercent);
        report += L"\r\nNVML init/count codes: " +
            std::to_wstring(initResult_) + L"/" + std::to_wstring(countResult_);
        report += L"\r\nNVML usage/temp/memory codes: " +
            std::to_wstring(utilizationResult_) + L"/" +
            std::to_wstring(temperatureResult_) + L"/" +
            std::to_wstring(memoryResult_);
        report += L"\r\nWin32 DLL load error: " + std::to_wstring(win32Error_);
        return report;
    }

    void Shutdown()
    {
        ready_ = false;
        device_ = nullptr;

        if (initialized_ && shutdown_)
            shutdown_();
        initialized_ = false;

        if (library_)
            FreeLibrary(library_);
        library_ = nullptr;
    }

private:
    using NvmlDevice = void*;
    struct NvmlUtilization { unsigned int gpu; unsigned int memory; };
    struct NvmlMemory
    {
        unsigned long long total;
        unsigned long long free;
        unsigned long long used;
    };

    using Init = int(__cdecl*)();
    using ShutdownFn = int(__cdecl*)();
    using DeviceCount = int(__cdecl*)(unsigned int*);
    using DeviceHandleByIndex = int(__cdecl*)(unsigned int, NvmlDevice*);
    using DeviceName = int(__cdecl*)(NvmlDevice, char*, unsigned int);
    using DeviceUtilization = int(__cdecl*)(NvmlDevice, NvmlUtilization*);
    using DeviceTemperature = int(__cdecl*)(NvmlDevice, int, unsigned int*);
    using DeviceMemory = int(__cdecl*)(NvmlDevice, NvmlMemory*);

    template<typename Fn>
    Fn Resolve(const char* symbol) const
    {
        return reinterpret_cast<Fn>(GetProcAddress(library_, symbol));
    }

    static std::wstring Value(int number)
    {
        return number >= 0 ? std::to_wstring(number) : L"--";
    }

    HMODULE library_ = nullptr;
    NvmlDevice device_ = nullptr;
    Init init_ = nullptr;
    ShutdownFn shutdown_ = nullptr;
    DeviceCount count_ = nullptr;
    DeviceHandleByIndex handle_ = nullptr;
    DeviceName name_ = nullptr;
    DeviceUtilization utilization_ = nullptr;
    DeviceTemperature temperature_ = nullptr;
    DeviceMemory memory_ = nullptr;
    bool initialized_ = false;
    bool ready_ = false;
    unsigned int deviceIndex_ = 0;
    int initResult_ = -1;
    int countResult_ = -1;
    int utilizationResult_ = -1;
    int temperatureResult_ = -1;
    int memoryResult_ = -1;
    DWORD win32Error_ = 0;
    std::wstring state_ = L"not started";
    std::string deviceName_;
    std::uint64_t usedVramMiB_ = 0;
    std::uint64_t totalVramMiB_ = 0;
};
