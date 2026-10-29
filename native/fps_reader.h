
#pragma once

#include <windows.h>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

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
        targetPid_.store(processId);
    }

    int GetFps(DWORD processId) const
    {
        if (processId == 0 || processId != reportedPid_.load())
            return -1;

        if (GetTickCount64() - lastFrame_.load() > 2500)
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

        {
            std::lock_guard<std::mutex> lock(processMutex_);

            if (process_ != nullptr)
                TerminateProcess(process_, 0);
        }

        worker_.join();

        if (launched_)
            StopSession();
    }

private:
    static std::vector<std::string> SplitCsv(const std::string& line)
    {
        std::vector<std::string> fields;
        std::string field;
        bool quoted = false;

        for (size_t i = 0; i < line.size(); ++i)
        {
            char ch = line[i];

            if (ch == '"')
            {
                if (quoted && i + 1 < line.size() && line[i + 1] == '"')
                {
                    field += '"';
                    ++i;
                }
                else
                {
                    quoted = !quoted;
                }
            }
            else if (ch == ',' && !quoted)
            {
                fields.push_back(field);
                field.clear();
            }
            else if (ch != '\r')
            {
                field += ch;
            }
        }

        fields.push_back(field);
        return fields;
    }

    void ReadFrames(HANDLE pipe)
    {
        std::string pending;
        char buffer[8192];

        int pidColumn = -1;
        int intervalColumn = -1;
        int previewLines = 0;
        DWORD smoothedPid = 0;
        double smoothedFps = 0;

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

            size_t newline;

            while ((newline = pending.find('\n')) != std::string::npos)
            {
                std::string line = pending.substr(0, newline);
                pending.erase(0, newline + 1);
                streamLines_.fetch_add(1, std::memory_order_relaxed);

                // Keep just the first few lines in RAM for troubleshooting.
                if (previewLines++ < 5)
                {
                    std::lock_guard<std::mutex> lock(diagnosticsMutex_);
                    firstOutput_ += line.substr(0, 200) + "\r\n";
                }

                auto fields = SplitCsv(line);

                if (pidColumn < 0 || intervalColumn < 0)
                {
                    pidColumn = -1;
                    intervalColumn = -1;

                    for (size_t i = 0; i < fields.size(); ++i)
                    {
                        if (fields[i] == "ProcessID")
                            pidColumn = static_cast<int>(i);

                        if (fields[i] == "MsBetweenPresents")
                            intervalColumn = static_cast<int>(i);
                    }

                    if (pidColumn >= 0 && intervalColumn >= 0)
                        headerFound_.store(true);

                    continue;
                }

                if (static_cast<int>(fields.size()) <= pidColumn ||
                    static_cast<int>(fields.size()) <= intervalColumn)
                    continue;

                char* pidEnd = nullptr;
                char* intervalEnd = nullptr;

                unsigned long pid = std::strtoul(
                    fields[pidColumn].c_str(), &pidEnd, 10);

                double milliseconds = std::strtod(
                    fields[intervalColumn].c_str(), &intervalEnd);

                if (*pidEnd != '\0' || *intervalEnd != '\0')
                    continue;

                parsedRows_.fetch_add(1, std::memory_order_relaxed);
                lastCsvPid_.store(static_cast<DWORD>(pid));

                if (pid == 0 || pid != targetPid_.load())
                    continue;

                targetRows_.fetch_add(1, std::memory_order_relaxed);

                if (!std::isfinite(milliseconds) ||
                    milliseconds <= 1 || milliseconds > 1000)
                    continue;

                double instantFps = 1000.0 / milliseconds;

                if (pid != smoothedPid ||
                    GetTickCount64() - lastFrame_.load() > 2500)
                {
                    smoothedFps = instantFps;
                    smoothedPid = static_cast<DWORD>(pid);
                }
                else
                {
                    smoothedFps = smoothedFps * 0.85 + instantFps * 0.15;
                }

                currentFps_.store(static_cast<int>(std::lround(smoothedFps)));
                validSamples_.fetch_add(1, std::memory_order_relaxed);
                reportedPid_.store(static_cast<DWORD>(pid));
                lastFrame_.store(GetTickCount64());
            }

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
            L"\" --output_stdout --no_console_stats --v2_metrics"
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
    std::mutex processMutex_;

    HANDLE process_ = nullptr;
    bool launched_ = false;

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
};
