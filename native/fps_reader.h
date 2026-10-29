
#pragma once

#include <windows.h>
#include <atomic>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <psapi.h>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include "frame_window.h"

class FpsReader
{
public:
    ~FpsReader()
    {
        Stop();
    }

    void Start()
    {
        launchState_.store(1);
        wchar_t module[MAX_PATH]{};
        DWORD length = GetModuleFileNameW(nullptr, module, MAX_PATH);

        if (length == 0 || length >= MAX_PATH)
        {
            launchState_.store(5);
            return;
        }

        std::wstring folder(module);
        size_t slash = folder.find_last_of(L"\\/");

        if (slash == std::wstring::npos)
        {
            launchState_.store(5);
            return;
        }

        executable_ = folder.substr(0, slash + 1)
            + L"PresentMon-2.4.1-x64.exe";

        if (GetFileAttributesW(executable_.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            launchError_.store(GetLastError());
            launchState_.store(6);
            return;
        }

        session_ = L"GameOverlayDev_" + std::to_wstring(GetCurrentProcessId());
        worker_ = std::thread([this]() { Run(); });
    }

    void SetTarget(DWORD processId)
    {
        // Drop the last displayed game's FPS immediately after an Alt+Tab.
        if (targetPid_.exchange(processId) != processId)
        {
            currentFps_.store(-1);
            reportedPid_.store(0);
            lastFrame_.store(0);
        }
    }

    int GetFps(DWORD processId) const
    {
        if (processId == 0 || processId != targetPid_.load() ||
            processId != reportedPid_.load())
            return -1;

        if (GetTickCount64() - lastFrame_.load() > 1500)
            return -1;

        return currentFps_.load();
    }

    std::wstring Diagnostics() const
    {
        std::wstring report = L"Game Overlay FPS diagnostics\r\n";
        const int state = launchState_.load();

        const wchar_t* status = L"not started";
        switch (state)
        {
        case 1: status = L"starting"; break;
        case 2: status = L"PresentMon running"; break;
        case 3: status = L"PresentMon exited"; break;
        case 4: status = L"startup failed"; break;
        case 5: status = L"could not locate GameOverlayDev.exe"; break;
        case 6: status = L"PresentMon EXE missing"; break;
        }

        DWORD foreground = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &foreground);

        report += L"Collector: ";
        report += status;
        report += L"\r\nForeground PID: " + std::to_wstring(foreground);
        report += L"\r\nTarget PID: " + std::to_wstring(targetPid_.load());
        report += L"\r\nLast CSV PID: " + std::to_wstring(lastCsvPid_.load());
        report += L"\r\nFPS PID: " + std::to_wstring(reportedPid_.load());
        report += L"\r\nCSV header found: ";
        report += headerFound_.load() ? L"yes" : L"no";
        report += L"\r\nStream lines: " + std::to_wstring(streamLines_.load());
        report += L"\r\nParsed PID rows: " + std::to_wstring(parsedRows_.load());
        report += L"\r\nRows for target: " + std::to_wstring(targetRows_.load());
        report += L"\r\nValid FPS samples: " + std::to_wstring(validSamples_.load());
        report += L"\r\nLaunch Win32 error: " + std::to_wstring(launchError_.load());
        report += L"\r\nPipe Win32 error: " + std::to_wstring(pipeError_.load());
        report += L"\r\nActive swap chains: " + std::to_wstring(activeSwapChains_.load());
        report += L"\r\nSelected swap chain: " + std::to_wstring(selectedSwapChain_.load());

        report += ProcessUsage(GetCurrentProcess(), L"Overlay");
        {
            std::lock_guard<std::mutex> lock(processMutex_);
            if (process_ != nullptr)
                report += ProcessUsage(process_, L"PresentMon");
        }

        HANDLE foregroundHandle = OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION, FALSE, foreground);
        if (foregroundHandle)
        {
            wchar_t name[MAX_PATH]{};
            DWORD length = MAX_PATH;
            if (QueryFullProcessImageNameW(foregroundHandle, 0, name, &length))
                report += L"\r\nForeground EXE: " + std::wstring(name, length);
            CloseHandle(foregroundHandle);
        }

        if (lastFrame_.load() != 0)
            report += L"\r\nLast FPS frame age (ms): "
                + std::to_wstring(GetTickCount64() - lastFrame_.load());

        if (state == 3)
            report += L"\r\nPresentMon exit code: " + std::to_wstring(exitCode_.load());

        {
            std::lock_guard<std::mutex> lock(diagnosticsMutex_);
            if (!firstOutput_.empty())
                report += L"\r\nFirst output lines:\r\n"
                    + std::wstring(firstOutput_.begin(), firstOutput_.end());
        }
        return report;
    }

    void Stop()
    {
        if (!worker_.joinable())
            return;

        stopping_.store(true);

        // Ask PresentMon to stop ETW first so it can release its session.
        if (launched_.load())
            StopSession();

        {
            std::lock_guard<std::mutex> lock(processMutex_);
            if (process_ && WaitForSingleObject(process_, 300) == WAIT_TIMEOUT)
                TerminateProcess(process_, 0);
        }

        worker_.join();
    }

private:
    static std::uint64_t Ticks(const FILETIME& time)
    {
        return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) |
               time.dwLowDateTime;
    }

    static std::wstring ProcessUsage(HANDLE process, const wchar_t* label)
    {
        std::wstring result;
        PROCESS_MEMORY_COUNTERS memory{};
        if (GetProcessMemoryInfo(process, &memory, sizeof(memory)))
            result += L"\r\n" + std::wstring(label) + L" working set (KB): " +
                std::to_wstring(memory.WorkingSetSize / 1024);

        FILETIME created{}, exited{}, kernel{}, user{}, now{};
        if (GetProcessTimes(process, &created, &exited, &kernel, &user))
        {
            GetSystemTimeAsFileTime(&now);
            const auto lifetime = Ticks(now) > Ticks(created)
                ? Ticks(now) - Ticks(created) : 0;
            const auto busy = Ticks(kernel) + Ticks(user);
            DWORD cores = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
            if (lifetime && cores)
            {
                double average = 100.0 * static_cast<double>(busy) /
                                 (static_cast<double>(lifetime) * cores);
                result += L"\r\n" + std::wstring(label) +
                    L" average CPU (%) since launch: " +
                    std::to_wstring(static_cast<int>(std::lround(average)));
            }
        }
        return result;
    }

    // Read a single CSV field without splitting every column into allocations.
    static std::string_view CsvField(std::string_view line, int requested)
    {
        int column = 0;
        size_t start = 0;
        bool quoted = false;

        for (size_t i = 0; i <= line.size(); ++i)
        {
            const char ch = i < line.size() ? line[i] : ',';

            if (ch == '"' && i < line.size())
            {
                if (quoted && i + 1 < line.size() && line[i + 1] == '"')
                {
                    ++i;
                    continue;
                }
                quoted = !quoted;
            }
            else if ((ch == ',' && !quoted) || i == line.size())
            {
                if (column == requested)
                {
                    auto value = line.substr(start, i - start);
                    if (value.size() > 1 && value.front() == '"' && value.back() == '"')
                    {
                        value.remove_prefix(1);
                        value.remove_suffix(1);
                    }
                    return value;
                }
                start = i + 1;
                ++column;
            }
        }
        return {};
    }

    struct SwapChain
    {
        FrameWindow window;
        ULONGLONG lastSeen = 0;
    };

    void ReadFrames(HANDLE pipe)
    {
        std::string pending;
        pending.reserve(32768);
        char buffer[32768]{};

        int pidColumn = -1;
        int swapColumn = -1;
        int intervalColumn = -1;
        int previewLines = 0;
        DWORD activePid = 0;
        std::uint64_t chosenChain = 0;
        std::unordered_map<std::uint64_t, SwapChain> chains;
        chains.reserve(8);

        while (!stopping_.load())
        {
            DWORD received = 0;
            if (!ReadFile(pipe, buffer, sizeof(buffer), &received, nullptr))
            {
                pipeError_.store(GetLastError());
                break;
            }
            if (received == 0)
                break;

            pending.append(buffer, received);
            size_t offset = 0;
            size_t newline;

            while ((newline = pending.find('\n', offset)) != std::string::npos)
            {
                std::string_view line(pending.data() + offset, newline - offset);
                offset = newline + 1;
                streamLines_.fetch_add(1, std::memory_order_relaxed);

                if (previewLines++ < 5)
                {
                    std::lock_guard<std::mutex> lock(diagnosticsMutex_);
                    firstOutput_ += std::string(line.substr(0, 200)) + "\r\n";
                }

                if (pidColumn < 0 || swapColumn < 0 || intervalColumn < 0)
                {
                    for (int column = 0; column < 32; ++column)
                    {
                        const auto field = CsvField(line, column);
                        if (field.empty())
                            break;
                        if (field == "ProcessID") pidColumn = column;
                        if (field == "SwapChainAddress") swapColumn = column;
                        if (field == "MsBetweenPresents") intervalColumn = column;
                    }
                    if (pidColumn >= 0 && swapColumn >= 0 && intervalColumn >= 0)
                        headerFound_.store(true);
                    continue;
                }

                DWORD desiredPid = targetPid_.load(std::memory_order_relaxed);
                if (desiredPid != activePid)
                {
                    chains.clear();
                    activePid = desiredPid;
                    chosenChain = 0;
                    selectedSwapChain_.store(0);
                    activeSwapChains_.store(0);
                }

                const auto pidField = CsvField(line, pidColumn);
                DWORD pid = 0;
                auto pidResult = std::from_chars(
                    pidField.data(), pidField.data() + pidField.size(), pid);
                if (pidResult.ec != std::errc{} ||
                    pidResult.ptr != pidField.data() + pidField.size())
                    continue;

                parsedRows_.fetch_add(1, std::memory_order_relaxed);
                lastCsvPid_.store(pid);

                if (pid == 0 || pid != desiredPid)
                    continue;

                targetRows_.fetch_add(1, std::memory_order_relaxed);

                auto swap = CsvField(line, swapColumn);
                if (swap.starts_with("0x") || swap.starts_with("0X"))
                    swap.remove_prefix(2);

                std::uint64_t address = 0;
                auto addressResult = std::from_chars(
                    swap.data(), swap.data() + swap.size(), address, 16);
                if (addressResult.ec != std::errc{} ||
                    addressResult.ptr != swap.data() + swap.size())
                    continue;

                const auto interval = CsvField(line, intervalColumn);
                double milliseconds = 0;
                auto valueResult = std::from_chars(
                    interval.data(), interval.data() + interval.size(),
                    milliseconds, std::chars_format::general);
                if (valueResult.ec != std::errc{} ||
                    valueResult.ptr != interval.data() + interval.size())
                    continue;

                const ULONGLONG now = GetTickCount64();
                if (!chains.contains(address) && chains.size() >= 8)
                {
                    // Keep a bounded number of swap chains per foreground game.
                    auto oldest = chains.begin();
                    for (auto it = chains.begin(); it != chains.end(); ++it)
                        if (it->second.lastSeen < oldest->second.lastSeen)
                            oldest = it;
                    chains.erase(oldest);
                }

                auto& chain = chains[address];
                if (now - chain.lastSeen > 1500)
                    chain.window = FrameWindow{};

                if (!chain.window.Add(milliseconds))
                    continue;

                chain.lastSeen = now;
                validSamples_.fetch_add(1, std::memory_order_relaxed);

                std::uint64_t bestAddress = 0;
                size_t bestCount = 0;
                int bestFps = -1;
                ULONGLONG bestSeen = 0;

                for (const auto& [id, candidate] : chains)
                {
                    if (now - candidate.lastSeen > 1500)
                        continue;
                    int candidateFps = candidate.window.Fps();
                    if (candidateFps < 0)
                        continue;

                    size_t count = candidate.window.Samples();
                    // Retain the previous chain unless another is noticeably busier.
                    if (count > bestCount ||
                        (count == bestCount && id == chosenChain))
                    {
                        bestAddress = id;
                        bestCount = count;
                        bestFps = candidateFps;
                        bestSeen = candidate.lastSeen;
                    }
                }

                if (chosenChain && chains.contains(chosenChain))
                {
                    const auto& previous = chains.at(chosenChain);
                    if (now - previous.lastSeen <= 1500 &&
                        previous.window.Fps() >= 0 &&
                        previous.window.Samples() * 10 >= bestCount * 9)
                    {
                        bestAddress = chosenChain;
                        bestFps = previous.window.Fps();
                        bestSeen = previous.lastSeen;
                    }
                }

                chosenChain = bestAddress;
                activeSwapChains_.store(static_cast<unsigned>(chains.size()));
                selectedSwapChain_.store(bestAddress);

                if (bestFps >= 0)
                {
                    currentFps_.store(bestFps);
                    reportedPid_.store(pid);
                    lastFrame_.store(bestSeen);
                }
            }

            // One erase per pipe read rather than reallocating for every frame.
            if (offset)
                pending.erase(0, offset);

            if (pending.size() > 65536)
                pending.clear();
        }
    }

    void Run()
    {
        SECURITY_ATTRIBUTES security{};
        security.nLength = sizeof(security);
        security.bInheritHandle = TRUE;

        HANDLE readPipe = nullptr;
        HANDLE writePipe = nullptr;

        if (!CreatePipe(&readPipe, &writePipe, &security, 0))
        {
            launchError_.store(GetLastError());
            launchState_.store(4);
            return;
        }

        SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

        HANDLE input = CreateFileW(
            L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;
        startup.hStdInput = input == INVALID_HANDLE_VALUE ? nullptr : input;
        startup.hStdOutput = writePipe;
        // Capture PresentMon's errors too, without creating log files.
        startup.hStdError = writePipe;

        std::wstring command = L"\"" + executable_ +
            L"\" --output_stdout --no_console_stats"
            L" --no_track_gpu --no_track_input --exclude_dropped"
            L" --session_name " + session_ +
            L" --stop_existing_session";

        PROCESS_INFORMATION info{};
        BOOL started = FALSE;

        {
            std::lock_guard<std::mutex> lock(processMutex_);

            if (!stopping_.load())
            {
                started = CreateProcessW(
                    executable_.c_str(), command.data(),
                    nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                    nullptr, nullptr, &startup, &info);

                if (started)
                {
                    process_ = info.hProcess;
                    launchState_.store(2);

                    // The job kills the child if the overlay crashes or exits.
                    HANDLE job = CreateJobObjectW(nullptr, nullptr);
                    if (job)
                    {
                        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
                        limits.BasicLimitInformation.LimitFlags =
                            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
                        if (SetInformationJobObject(
                            job, JobObjectExtendedLimitInformation,
                            &limits, sizeof(limits)) &&
                            AssignProcessToJobObject(job, info.hProcess))
                        {
                            job_ = job;
                        }
                        else
                        {
                            CloseHandle(job);
                        }
                    }
                }
                else
                {
                    launchError_.store(GetLastError());
                    launchState_.store(4);
                }
            }
        }

        CloseHandle(writePipe);

        if (input != INVALID_HANDLE_VALUE)
            CloseHandle(input);

        if (!started)
        {
            CloseHandle(readPipe);
            return;
        }

        launched_ = true;
        CloseHandle(info.hThread);

        ReadFrames(readPipe);
        CloseHandle(readPipe);

        {
            std::lock_guard<std::mutex> lock(processMutex_);

            if (WaitForSingleObject(info.hProcess, 0) == WAIT_TIMEOUT)
                TerminateProcess(info.hProcess, 0);

            DWORD code = 0;
            if (GetExitCodeProcess(info.hProcess, &code))
                exitCode_.store(code);

            process_ = nullptr;
            if (job_)
            {
                CloseHandle(job_);
                job_ = nullptr;
            }
        }

        launchState_.store(3);
        CloseHandle(info.hProcess);
    }

    void StopSession()
    {
        std::wstring command = L"\"" + executable_ +
            L"\" --terminate_existing_session --session_name " + session_;

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);

        PROCESS_INFORMATION info{};

        if (CreateProcessW(
            executable_.c_str(), command.data(),
            nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
            nullptr, nullptr, &startup, &info))
        {
            CloseHandle(info.hThread);

            if (WaitForSingleObject(info.hProcess, 2000) == WAIT_TIMEOUT)
                TerminateProcess(info.hProcess, 0);

            CloseHandle(info.hProcess);
        }
    }

    std::wstring executable_;
    std::wstring session_;
    std::thread worker_;
    mutable std::mutex processMutex_;

    HANDLE process_ = nullptr;
    HANDLE job_ = nullptr;
    std::atomic<bool> launched_{false};

    std::atomic<bool> stopping_{false};
    std::atomic<DWORD> targetPid_{0};
    std::atomic<DWORD> reportedPid_{0};
    std::atomic<int> currentFps_{-1};
    std::atomic<ULONGLONG> lastFrame_{0};

    mutable std::mutex diagnosticsMutex_;
    std::string firstOutput_;
    std::atomic<int> launchState_{0};
    std::atomic<DWORD> launchError_{0};
    std::atomic<DWORD> pipeError_{0};
    std::atomic<DWORD> exitCode_{0};
    std::atomic<DWORD> lastCsvPid_{0};
    std::atomic<bool> headerFound_{false};
    std::atomic<unsigned long long> streamLines_{0};
    std::atomic<unsigned long long> parsedRows_{0};
    std::atomic<unsigned long long> targetRows_{0};
    std::atomic<unsigned long long> validSamples_{0};
    std::atomic<unsigned> activeSwapChains_{0};
    std::atomic<std::uint64_t> selectedSwapChain_{0};
};
