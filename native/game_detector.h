// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <windows.h>
#include <cwctype>
#include <string>

class GameDetector
{
public:
    bool IsLikelyGame(HWND foreground)
    {
        if (!foreground || !IsWindowVisible(foreground) ||
            foreground == GetShellWindow())
            return false;

        DWORD pid = 0;
        GetWindowThreadProcessId(foreground, &pid);
        if (!pid)
            return false;

        if (pid == lastPid_)
            return lastResult_;

        lastPid_ = pid;
        lastResult_ = false;

        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                     FALSE, pid);
        if (!process)
            return false;

        wchar_t buffer[MAX_PATH]{};
        DWORD count = MAX_PATH;
        const BOOL found = QueryFullProcessImageNameW(
            process, 0, buffer, &count);
        CloseHandle(process);

        if (!found || count == 0)
            return false;

        std::wstring filename(buffer, count);
        const size_t slash = filename.find_last_of(L"\\/");
        if (slash != std::wstring::npos)
            filename.erase(0, slash + 1);

        for (wchar_t& ch : filename)
            ch = static_cast<wchar_t>(towlower(ch));

        const wchar_t* blocked[] = {
            L"spotify.exe", L"spotifast.exe", L"spotify", L"spotifast",
            L"discord.exe", L"chrome.exe", L"msedge.exe",
            L"firefox.exe", L"brave.exe", L"opera.exe",
            L"steamwebhelper.exe", L"steam.exe",
            L"explorer.exe", L"taskmgr.exe", L"code.exe",
            L"notepad.exe", L"notepad++.exe", L"winword.exe",
            L"excel.exe", L"powerpnt.exe", L"vlc.exe",
            L"obs64.exe", L"windowsterminal.exe",
            L"telegram.exe", L"whatsapp.exe",
            L"gameoverlaydev.exe"
        };

        for (const wchar_t* name : blocked)
        {
            if (filename == name)
                return false;
        }

        if (filename.find(L"spotify") != std::wstring::npos ||
            filename.find(L"spotifast") != std::wstring::npos)
            return false;

        lastResult_ = true;
        return true;
    }

private:
    DWORD lastPid_ = 0;
    bool lastResult_ = false;
};
