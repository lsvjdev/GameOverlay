
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 lsvjdev

#include <windows.h>
#include <cstdio>
#include <cwchar>
#include <cstring>
#include "system_metrics.h"
#include "fps_reader.h"

SystemMetrics metrics;
FpsReader fpsReader;

constexpr int ToggleHotkey = 1;
constexpr int ExitHotkey = 2;
constexpr int DiagnosticsHotkey = 3;

void CopyFpsDiagnostics(HWND window)
{
    const std::wstring report = fpsReader.Diagnostics();
    bool copied = false;

    if (OpenClipboard(window))
    {
        if (EmptyClipboard())
        {
            const SIZE_T bytes = (report.size() + 1) * sizeof(wchar_t);
            HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);

            if (memory != nullptr)
            {
                void* data = GlobalLock(memory);

                if (data != nullptr)
                {
                    memcpy(data, report.c_str(), bytes);
                    GlobalUnlock(memory);

                    if (SetClipboardData(CF_UNICODETEXT, memory))
                    {
                        copied = true;
                        memory = nullptr;
                    }
                }

                if (memory != nullptr)
                    GlobalFree(memory);
            }
        }

        CloseClipboard();
    }

    MessageBoxW(window,
                copied ? L"FPS diagnostics copied. Paste them in ChatGPT."
                       : L"Could not copy FPS diagnostics.",
                L"Game Overlay Dev", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wp, LPARAM lp)
{
    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);

        RECT area;
        GetClientRect(window, &area);

        HBRUSH background = CreateSolidBrush(RGB(24, 25, 28));
        FillRect(dc, &area, background);
        DeleteObject(background);

        SYSTEMTIME time;
        GetLocalTime(&time);

        wchar_t timeText[32];
        wchar_t cpuText[16];
        wchar_t ramText[16];
        wchar_t fpsText[16];

        DWORD foregroundPid = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPid);

        int fps = fpsReader.GetFps(foregroundPid);

        if (fps >= 0)
            swprintf_s(fpsText, L"%d", fps);
        else
            wcscpy_s(fpsText, L"--");

        int hour = time.wHour % 12;
        if (hour == 0) hour = 12;

        swprintf_s(timeText, L"%02d:%02d:%02d %ls",
                   hour, time.wMinute, time.wSecond,
                   time.wHour >= 12 ? L"PM" : L"AM");

        if (metrics.CpuPercent >= 0)
            swprintf_s(cpuText, L"%d%%", metrics.CpuPercent);
        else
            wcscpy_s(cpuText, L"--");

        if (metrics.RamPercent >= 0)
            swprintf_s(ramText, L"%d%%", metrics.RamPercent);
        else
            wcscpy_s(ramText, L"--");

        HFONT font = CreateFontW(
            -16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH, L"Segoe UI");

        HGDIOBJ previous = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);

        int x = 10;
        const int y = 5;

        auto DrawMetric = [&](const wchar_t* label, const wchar_t* value)
        {
            SIZE size{};

            SetTextColor(dc, RGB(189, 181, 188));

            int length = lstrlenW(label);
            TextOutW(dc, x, y, label, length);
            GetTextExtentPoint32W(dc, label, length, &size);

            x += size.cx + 4;

            SetTextColor(dc, RGB(82, 217, 85));

            length = lstrlenW(value);
            TextOutW(dc, x, y, value, length);
            GetTextExtentPoint32W(dc, value, length, &size);

            x += size.cx + 16;
        };

        DrawMetric(L"FPS", fpsText);
        DrawMetric(L"Time", timeText);
        DrawMetric(L"CPU", cpuText);
        DrawMetric(L"RAM", ramText);

        // Each metric updates x with its measured text width. Expand the
        // strip only when necessary so changing values do not make it jitter.
        const int requiredWidth = x - 16 + 10;

        SelectObject(dc, previous);
        DeleteObject(font);
        EndPaint(window, &paint);

        if (requiredWidth > area.right)
        {
            SetWindowPos(window, nullptr, 0, 0, requiredWidth, area.bottom,
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            InvalidateRect(window, nullptr, FALSE);
        }

        return 0;
    }

    case WM_TIMER:
    {
        DWORD foregroundPid = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPid);

        fpsReader.SetTarget(foregroundPid);
        metrics.Refresh();
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    }

    case WM_HOTKEY:
        if (wp == ToggleHotkey)
            ShowWindow(window, IsWindowVisible(window)
                        ? SW_HIDE : SW_SHOWNOACTIVATE);
        else if (wp == ExitHotkey)
            DestroyWindow(window);
        else if (wp == DiagnosticsHotkey)
            CopyFpsDiagnostics(window);
        return 0;

    case WM_DESTROY:
        KillTimer(window, 1);
        UnregisterHotKey(window, ToggleHotkey);
        UnregisterHotKey(window, ExitHotkey);
        UnregisterHotKey(window, DiagnosticsHotkey);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(window, message, wp, lp);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    WNDCLASSW cls{};
    cls.lpfnWndProc = WindowProc;
    cls.hInstance = instance;
    cls.lpszClassName = L"GameOverlayNativeDev";

    if (!RegisterClassW(&cls))
        return 1;

    HWND window = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW |
        WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        cls.lpszClassName, L"Game Overlay Dev",
        WS_POPUP, 0, 0, 360, 28,
        nullptr, nullptr, instance, nullptr);

    if (!window)
        return 1;

    SetLayeredWindowAttributes(window, 0, 235, LWA_ALPHA);

    if (!RegisterHotKey(window, ToggleHotkey,
                        MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'O') ||
        !RegisterHotKey(window, ExitHotkey,
                        MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'Q'))
    {
        MessageBoxW(nullptr, L"Could not register hotkeys.",
                    L"Game Overlay Dev", MB_OK | MB_ICONERROR);
        DestroyWindow(window);
        return 1;
    }

    // Optional diagnostics shortcut: Ctrl + Shift + D.
    RegisterHotKey(window, DiagnosticsHotkey,
                   MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'D');

    metrics.Refresh();
    fpsReader.Start();
    SetTimer(window, 1, 1000, nullptr);
    ShowWindow(window, SW_SHOWNOACTIVATE);

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    fpsReader.Stop();
    return 0;
}
