
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
        wchar_t module[MAX_PATH]{};
        DWORD length = GetModuleFileNameW(nullptr, module, MAX_PATH);

        if (length == 0 || length >= MAX_PATH)
            return;

        std::wstring folder(module);
        size_t slash = folder.find_last_of(L"\\/");

        if (slash == std::wstring::npos)
            return;

        executable_ = folder.substr(0, slash + 1)
            + L"PresentMon-2.4.1-x64.exe";

        if (GetFileAttributesW(executable_.c_str()) == INVALID_FILE_ATTRIBUTES)
            return;

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
        DWORD smoothedPid = 0;
        double smoothedFps = 0;

        while (!stopping_.load())
        {
            DWORD received = 0;

            if (!ReadFile(pipe, buffer, sizeof(buffer), &received, nullptr)
                || received == 0)
                break;

            pending.append(buffer, received);

            size_t newline;

            while ((newline = pending.find('\n')) != std::string::npos)
            {
                std::string line = pending.substr(0, newline);
                pending.erase(0, newline + 1);

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

                if (pid == 0 || pid != targetPid_.load())
                    continue;

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
            return;

        SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

        HANDLE input = CreateFileW(
            L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

        HANDLE errorOutput = CreateFileW(
            L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;
        startup.hStdInput = input == INVALID_HANDLE_VALUE ? nullptr : input;
        startup.hStdOutput = writePipe;
        startup.hStdError = errorOutput == INVALID_HANDLE_VALUE
            ? writePipe : errorOutput;

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
                    process_ = info.hProcess;
            }
        }

        CloseHandle(writePipe);

        if (input != INVALID_HANDLE_VALUE)
            CloseHandle(input);

        if (errorOutput != INVALID_HANDLE_VALUE)
            CloseHandle(errorOutput);

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

            process_ = nullptr;
        }

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
};
