// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include "cpu_temperature_math.h"

class CpuTemperature
{
public:
    int Celsius = -1;

    CpuTemperature() = default;
    CpuTemperature(const CpuTemperature&) = delete;
    CpuTemperature& operator=(const CpuTemperature&) = delete;

    ~CpuTemperature()
    {
        Shutdown();
    }

    bool Start()
    {
        if (library_)
            return ready_;

        wchar_t path[MAX_PATH]{};
        const DWORD length = GetEnvironmentVariableW(
            L"ProgramFiles", path, MAX_PATH);

        if (length == 0 || length >= MAX_PATH)
        {
            state_ = L"PawnIO library location unavailable";
            return false;
        }

        const std::wstring dllPath =
            std::wstring(path) + L"\\PawnIO\\PawnIOLib.dll";

        library_ = LoadLibraryExW(dllPath.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);

        if (!library_)
        {
            win32Error_ = GetLastError();
            state_ = L"PawnIOLib.dll unavailable; install PawnIO";
            return false;
        }

        open_ = reinterpret_cast<OpenFn>(
            GetProcAddress(library_, "pawnio_open"));
        load_ = reinterpret_cast<LoadFn>(
            GetProcAddress(library_, "pawnio_load"));
        execute_ = reinterpret_cast<ExecuteFn>(
            GetProcAddress(library_, "pawnio_execute"));
        close_ = reinterpret_cast<CloseFn>(
            GetProcAddress(library_, "pawnio_close"));

        if (!open_ || !load_ || !execute_ || !close_)
        {
            state_ = L"PawnIOLib.dll missing required functions";
            Shutdown();
            return false;
        }

        if (open_(&handle_) != S_OK || !handle_)
        {
            state_ = L"Could not open PawnIO driver";
            Shutdown();
            return false;
        }

        wchar_t exePath[MAX_PATH]{};
        const DWORD exeLength = GetModuleFileNameW(
            nullptr, exePath, MAX_PATH);

        if (exeLength == 0 || exeLength >= MAX_PATH)
        {
            state_ = L"Executable path unavailable";
            Shutdown();
            return false;
        }

        const std::wstring executable(exePath, exeLength);
        const size_t slash = executable.find_last_of(L"\\/");

        if (slash == std::wstring::npos)
        {
            state_ = L"Executable directory unavailable";
            Shutdown();
            return false;
        }

        const std::wstring modulePath = executable.substr(0, slash + 1) +
            L"bin\\modules\\AMDFamily17.bin";
        HANDLE file = CreateFileW(modulePath.c_str(), GENERIC_READ,
            FILE_SHARE_READ, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);

        if (file == INVALID_HANDLE_VALUE)
        {
            win32Error_ = GetLastError();
            state_ = L"AMDFamily17.bin module missing";
            Shutdown();
            return false;
        }

        LARGE_INTEGER size{};
        const bool validSize = GetFileSizeEx(file, &size) &&
            size.QuadPart > 0 && size.QuadPart <= 1024 * 1024;

        std::vector<unsigned char> blob;
        if (validSize)
            blob.resize(static_cast<size_t>(size.QuadPart));

        DWORD bytesRead = 0;
        const bool loadedFile = validSize &&
            ReadFile(file, blob.data(), static_cast<DWORD>(blob.size()),
                &bytesRead, nullptr) &&
            bytesRead == blob.size();

        CloseHandle(file);

        if (!loadedFile)
        {
            state_ = L"Could not read PawnIO module";
            Shutdown();
            return false;
        }

        loadResult_ = load_(handle_, blob.data(), blob.size());
        if (FAILED(loadResult_))
        {
            state_ = L"PawnIO rejected AMD sensor module";
            Shutdown();
            return false;
        }

        pciMutex_ = CreateMutexW(nullptr, FALSE, L"Global\\Access_PCI");
        if (!pciMutex_)
        {
            win32Error_ = GetLastError();
            state_ = L"Could not open PCI access mutex";
            Shutdown();
            return false;
        }

        ready_ = true;
        state_ = L"PawnIO AMD sensor ready";
        Refresh();
        return true;
    }

    void Refresh()
    {
        Celsius = -1;
        if (!ready_)
            return;

        const DWORD wait = WaitForSingleObject(pciMutex_, 25);
        if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED)
        {
            state_ = L"PCI access mutex unavailable";
            return;
        }

        const std::uint64_t offset = 0x00059800ULL;
        std::uint64_t raw = 0;
        SIZE_T returned = 0;

        const HRESULT result = execute_(
            handle_, "ioctl_read_smn", &offset, 1, &raw, 1, &returned);

        ReleaseMutex(pciMutex_);

        readResult_ = result;
        if (FAILED(result) || returned != 1)
        {
            state_ = L"AMD temperature sensor read failed";
            return;
        }

        rawReading_ = static_cast<std::uint32_t>(raw);
        Celsius = DecodeAmdTemperature(rawReading_);
        state_ = Celsius >= 0
            ? L"PawnIO AMD sensor ready"
            : L"AMD sensor reading unavailable";
    }

    std::wstring Diagnostics() const
    {
        std::wstring report = L"CPU temperature diagnostics\r\nStatus: " + state_;
        report += L"\r\nBackend: PawnIO AMD temperature module";
        report += L"\r\nCPU temperature (C): ";
        report += Celsius < 0 ? L"--" : std::to_wstring(Celsius);
        report += L"\r\nRaw SMN value: " + std::to_wstring(rawReading_);
        report += L"\r\nModule result: " +
            std::to_wstring(static_cast<unsigned long>(loadResult_));
        report += L"\r\nRead result: " +
            std::to_wstring(static_cast<unsigned long>(readResult_));
        report += L"\r\nWin32 error: " + std::to_wstring(win32Error_);
        return report;
    }

    void Shutdown()
    {
        ready_ = false;
        Celsius = -1;

        if (pciMutex_)
            CloseHandle(pciMutex_);
        pciMutex_ = nullptr;

        if (handle_ && close_)
            close_(handle_);
        handle_ = nullptr;

        if (library_)
            FreeLibrary(library_);
        library_ = nullptr;
    }

private:
    using OpenFn = HRESULT(STDAPICALLTYPE*)(PHANDLE);
    using LoadFn = HRESULT(STDAPICALLTYPE*)(HANDLE, const UCHAR*, SIZE_T);
    using ExecuteFn = HRESULT(STDAPICALLTYPE*)(
        HANDLE, PCSTR, const ULONG64*, SIZE_T,
        PULONG64, SIZE_T, PSIZE_T);
    using CloseFn = HRESULT(STDAPICALLTYPE*)(HANDLE);

    HMODULE library_ = nullptr;
    HANDLE handle_ = nullptr;
    HANDLE pciMutex_ = nullptr;
    OpenFn open_ = nullptr;
    LoadFn load_ = nullptr;
    ExecuteFn execute_ = nullptr;
    CloseFn close_ = nullptr;
    bool ready_ = false;
    HRESULT loadResult_ = S_OK;
    HRESULT readResult_ = S_OK;
    DWORD win32Error_ = 0;
    std::uint32_t rawReading_ = 0;
    std::wstring state_ = L"not started";
};
