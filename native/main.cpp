
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 lsvjdev

#include <windows.h>
#include <cstdio>
#include <cwchar>
#include <cstring>
#include <shellapi.h>
#include "settings_config.h"
#include "settings_window.h"
#include "game_detector.h"
#include "system_metrics.h"
#include "gpu_metrics.h"
#include "cpu_temperature.h"
#include "fps_reader.h"

AppSettings settings;
SettingsWindow settingsWindow(settings);
GameDetector gameDetector;
SystemMetrics metrics;
GpuMetrics gpuMetrics;
CpuTemperature cpuTemperature;
FpsReader fpsReader;
HFONT overlayFont = nullptr;
bool overlayFontTransparent = false;

struct OverlayReadings
{
    wchar_t timeText[32]{};
    wchar_t cpuText[40]{};
    wchar_t ramText[16]{};
    wchar_t fpsText[16]{};
    wchar_t gpuText[40]{};
    wchar_t vramText[16]{};

    static const wchar_t* Label(unsigned metric)
    {
        switch (metric)
        {
        case MetricFps: return L"FPS";
        case MetricTime: return L"Time";
        case MetricCpu: return L"CPU";
        case MetricGpu: return L"GPU";
        case MetricVram: return L"VRAM";
        case MetricRam: return L"RAM";
        }
        return L"";
    }

    const wchar_t* Value(unsigned metric) const
    {
        switch (metric)
        {
        case MetricFps: return fpsText;
        case MetricTime: return timeText;
        case MetricCpu: return cpuText;
        case MetricGpu: return gpuText;
        case MetricVram: return vramText;
        case MetricRam: return ramText;
        }
        return L"";
    }
};

#include "transparent_overlay.h"

TransparentOverlayRenderer transparentRenderer;

OverlayReadings CaptureOverlayReadings()
{
    OverlayReadings readings{};
    SYSTEMTIME time;
    GetLocalTime(&time);

    DWORD foregroundPid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPid);

    int fps = fpsReader.GetFps(foregroundPid);

    if (fps >= 0)
        swprintf_s(readings.fpsText, L"%d", fps);
    else
        wcscpy_s(readings.fpsText, L"--");

    if (settings.clock12)
    {
        int hour = time.wHour % 12;
        if (hour == 0) hour = 12;

        swprintf_s(readings.timeText, L"%02d:%02d:%02d %ls",
                   hour, time.wMinute, time.wSecond,
                   time.wHour >= 12 ? L"PM" : L"AM");
    }
    else
    {
        swprintf_s(readings.timeText, L"%02d:%02d:%02d",
                   time.wHour, time.wMinute, time.wSecond);
    }

    if (metrics.CpuPercent >= 0 && cpuTemperature.Celsius >= 0 &&
        metrics.CpuFrequencyMhz >= 0)
        swprintf_s(readings.cpuText, L"%d%% %d\u00B0C %dMHz",
            metrics.CpuPercent, cpuTemperature.Celsius, metrics.CpuFrequencyMhz);
    else if (metrics.CpuPercent >= 0 && cpuTemperature.Celsius >= 0)
        swprintf_s(readings.cpuText, L"%d%% %d\u00B0C",
            metrics.CpuPercent, cpuTemperature.Celsius);
    else if (metrics.CpuPercent >= 0 && metrics.CpuFrequencyMhz >= 0)
        swprintf_s(readings.cpuText, L"%d%% %dMHz",
            metrics.CpuPercent, metrics.CpuFrequencyMhz);
    else if (metrics.CpuPercent >= 0)
        swprintf_s(readings.cpuText, L"%d%%", metrics.CpuPercent);
    else
        wcscpy_s(readings.cpuText, L"--");

    if (metrics.RamPercent >= 0)
        swprintf_s(readings.ramText, L"%d%%", metrics.RamPercent);
    else
        wcscpy_s(readings.ramText, L"--");

    if (gpuMetrics.UsagePercent >= 0 && gpuMetrics.TemperatureC >= 0)
        swprintf_s(readings.gpuText, L"%d%% %d\u00B0C",
                   gpuMetrics.UsagePercent, gpuMetrics.TemperatureC);
    else if (gpuMetrics.UsagePercent >= 0)
        swprintf_s(readings.gpuText, L"%d%%", gpuMetrics.UsagePercent);
    else if (gpuMetrics.TemperatureC >= 0)
        swprintf_s(readings.gpuText, L"%d\u00B0C", gpuMetrics.TemperatureC);
    else
        wcscpy_s(readings.gpuText, L"--");

    if (gpuMetrics.VramPercent >= 0)
        swprintf_s(readings.vramText, L"%d%%", gpuMetrics.VramPercent);
    else
        wcscpy_s(readings.vramText, L"--");
    return readings;
}

constexpr int ToggleHotkey = 1;
constexpr int ExitHotkey = 2;
constexpr int DiagnosticsHotkey = 3;
constexpr UINT TrayMessage = WM_APP + 1;
constexpr UINT SettingsChanged = WM_APP + 2;

void ApplyOverlayState(HWND window)
{
    static int lastOpacity = -1;
    static bool lastAlphaMode = false;

    const bool alphaMode = settings.transparent && transparentRenderer.Ready();
    const int opacity = settings.transparent ? 100 : settings.opacity;

    if (lastOpacity != opacity || lastAlphaMode != alphaMode)
    {
        if (lastOpacity >= 0 && lastAlphaMode != alphaMode)
        {
            const LONG_PTR style = GetWindowLongPtrW(window, GWL_EXSTYLE);
            SetWindowLongPtrW(window, GWL_EXSTYLE, style & ~WS_EX_LAYERED);
            SetWindowLongPtrW(window, GWL_EXSTYLE, style);
        }

        if (!alphaMode)
        {
            SetLayeredWindowAttributes(
                window, RGB(24, 25, 28),
                static_cast<BYTE>(opacity * 255 / 100),
                settings.transparent ? LWA_COLORKEY : LWA_ALPHA);
        }

        lastOpacity = opacity;
        lastAlphaMode = alphaMode;
    }

    DWORD foregroundPid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPid);

    const bool visible = settings.enabled &&
        (!settings.gameOnly ||
         (gameDetector.IsLikelyGame(GetForegroundWindow()) &&
          fpsReader.GetFps(foregroundPid) >= 0));

    if (alphaMode)
    {
        if (visible)
            transparentRenderer.Render(window, settings, CaptureOverlayReadings());
    }
    else
        InvalidateRect(window, nullptr, FALSE);

    if (visible != (IsWindowVisible(window) != FALSE))
        ShowWindow(window, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
}

void CopyFpsDiagnostics(HWND window)
{
    const std::wstring report = fpsReader.Diagnostics() +
        L"\r\n\r\n" + gpuMetrics.Diagnostics() +
        L"\r\n\r\n" + cpuTemperature.Diagnostics();
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

        if (settings.transparent && transparentRenderer.Ready())
        {
            EndPaint(window, &paint);
            return 0;
        }

        RECT area;
        GetClientRect(window, &area);

        HBRUSH background = CreateSolidBrush(RGB(24, 25, 28));
        FillRect(dc, &area, background);
        DeleteObject(background);

        const auto readings = CaptureOverlayReadings();

        if (!overlayFont || overlayFontTransparent != settings.transparent)
        {
            if (overlayFont)
                DeleteObject(overlayFont);

            overlayFont = CreateFontW(
                -16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                settings.transparent ? NONANTIALIASED_QUALITY : CLEARTYPE_QUALITY,
                DEFAULT_PITCH, L"Segoe UI");

            overlayFontTransparent = settings.transparent;
        }

        HGDIOBJ previous = SelectObject(dc, overlayFont);
        SetBkMode(dc, TRANSPARENT);

        int x = 10;
        const int y = 5;

        auto DrawMetric = [&](const wchar_t* label, const wchar_t* value)
        {
            SIZE size{};

            SetTextColor(dc, settings.transparent
                ? RGB(238, 239, 241) : RGB(189, 181, 188));

            int length = lstrlenW(label);
            TextOutW(dc, x, y, label, length);
            GetTextExtentPoint32W(dc, label, length, &size);

            x += size.cx + 4;

            SetTextColor(dc, settings.transparent
                ? RGB(255, 255, 255) : RGB(82, 217, 85));

            length = lstrlenW(value);
            TextOutW(dc, x, y, value, length);
            GetTextExtentPoint32W(dc, value, length, &size);

            x += size.cx + 16;
        };

        for (unsigned metric : settings.order)
        {
            if (settings.Has(metric))
                DrawMetric(OverlayReadings::Label(metric),
                           readings.Value(metric));
        }

        const int requiredWidth = x == 10 ? 160 : x - 16 + 10;

        SelectObject(dc, previous);
        EndPaint(window, &paint);

        const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        const int targetWidth = requiredWidth > screenWidth
            ? screenWidth : requiredWidth;
        const int horizontal = settings.position % 3;
        const int targetX = horizontal == 0 ? 0 :
            (horizontal == 1 ? (screenWidth - targetWidth) / 2 :
                               screenWidth - targetWidth);
        const int targetY = settings.position < 3
            ? 0 : screenHeight - area.bottom;

        RECT current{};
        GetWindowRect(window, &current);

        if (current.left != targetX || current.top != targetY ||
            current.right - current.left != targetWidth)
        {
            SetWindowPos(window, nullptr, targetX, targetY,
                targetWidth, area.bottom, SWP_NOZORDER | SWP_NOACTIVATE);
        }

        return 0;
    }

    case WM_TIMER:
    {
        DWORD foregroundPid = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPid);

        fpsReader.SetTarget(foregroundPid);

        // Check foreground games twice a second; hardware usage once a second.
        static ULONGLONG lastMetricsTick = 0;
        const ULONGLONG now = GetTickCount64();
        if (now - lastMetricsTick >= 1000)
        {
            metrics.Refresh();
            gpuMetrics.Refresh();
            cpuTemperature.Refresh();
            lastMetricsTick = now;
        }

        ApplyOverlayState(window);
        return 0;
    }

    case SettingsChanged:
        ApplyOverlayState(window);
        return 0;

    case TrayMessage:
        if (lp == WM_LBUTTONDBLCLK)
        {
            settingsWindow.Show(window);
        }
        else if (lp == WM_RBUTTONUP || lp == WM_CONTEXTMENU)
        {
            POINT point{};
            GetCursorPos(&point);

            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, 101, L"Open settings");
            AppendMenuW(menu, MF_STRING, 102,
                settings.enabled ? L"Hide overlay" : L"Show overlay");
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(menu, MF_STRING, 103, L"Exit");

            SetForegroundWindow(window);
            const UINT action = TrackPopupMenu(menu,
                TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
                point.x, point.y, 0, window, nullptr);
            DestroyMenu(menu);
            PostMessageW(window, WM_NULL, 0, 0);

            if (action == 101)
                settingsWindow.Show(window);
            else if (action == 102)
            {
                settings.enabled = !settings.enabled;
                settings.Save();
                ApplyOverlayState(window);
            }
            else if (action == 103)
                DestroyWindow(window);
        }
        return 0;

    case WM_HOTKEY:
        if (wp == ToggleHotkey)
        {
            settings.enabled = !settings.enabled;
            settings.Save();
            ApplyOverlayState(window);
            settingsWindow.Refresh();
        }
        else if (wp == ExitHotkey)
            DestroyWindow(window);
        else if (wp == DiagnosticsHotkey)
            CopyFpsDiagnostics(window);
        return 0;

    case WM_DESTROY:
        KillTimer(window, 1);
        if (overlayFont)
        {
            DeleteObject(overlayFont);
            overlayFont = nullptr;
        }
        UnregisterHotKey(window, ToggleHotkey);
        UnregisterHotKey(window, ExitHotkey);
        UnregisterHotKey(window, DiagnosticsHotkey);

        NOTIFYICONDATAW tray{};
        tray.cbSize = sizeof(tray);
        tray.hWnd = window;
        tray.uID = 1;
        Shell_NotifyIconW(NIM_DELETE, &tray);

        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(window, message, wp, lp);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    settings.Load();
    transparentRenderer.Start();

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

    NOTIFYICONDATAW tray{};
    tray.cbSize = sizeof(tray);
    tray.hWnd = window;
    tray.uID = 1;
    tray.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    tray.uCallbackMessage = TrayMessage;
    tray.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(tray.szTip, L"Game Overlay Dev");
    Shell_NotifyIconW(NIM_ADD, &tray);

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
    gpuMetrics.Start();
    cpuTemperature.Start();
    DWORD initialPid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &initialPid);
    fpsReader.SetTarget(initialPid);
    fpsReader.Start();
    SetTimer(window, 1, 500, nullptr);
    ApplyOverlayState(window);

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    fpsReader.Stop();
    gpuMetrics.Shutdown();
    cpuTemperature.Shutdown();
    transparentRenderer.Stop();
    return 0;
}
