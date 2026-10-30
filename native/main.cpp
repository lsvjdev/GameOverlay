
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 lsvjdev

#include <windows.h>
#include <cstdio>
#include <cwchar>
#include <cstring>
#include <array>
#include <string>
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
    struct Part
    {
        std::wstring value;
        std::wstring reserve;
    };
    struct MetricValue
    {
        std::array<Part, 3> parts{};
        int count = 0;
    };
    std::array<MetricValue, 6> values{};

    void Add(unsigned metric, const std::wstring& value,
             const std::wstring& reserve)
    {
        const int index = AppSettings::MetricIndex(metric);
        if (index < 0)
            return;
        auto& entry = values[index];
        if (entry.count < static_cast<int>(entry.parts.size()))
            entry.parts[entry.count++] = {value, reserve};
    }

    const MetricValue& Get(unsigned metric) const
    {
        const int index = AppSettings::MetricIndex(metric);
        return values[index >= 0 ? index : 0];
    }

    // Used only if GDI+ initialization fails.
    std::wstring Value(unsigned metric) const
    {
        std::wstring result;
        const auto& entry = Get(metric);
        for (int i = 0; i < entry.count; ++i)
        {
            if (i) result += L" ";
            result += entry.parts[i].value;
        }
        return result.empty() ? L"--" : result;
    }

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
};

#include "transparent_overlay.h"

TransparentOverlayRenderer transparentRenderer;

OverlayReadings CaptureOverlayReadings()
{
    OverlayReadings readings{};
    SYSTEMTIME time{};
    GetLocalTime(&time);

    DWORD foregroundPid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPid);
    const int fps = fpsReader.GetFps(foregroundPid);

    readings.Add(MetricFps,
        fps >= 0 ? std::to_wstring(fps) : L"--", L"88888");

    wchar_t buffer[128]{};
    if (settings.clock12)
    {
        int hour = time.wHour % 12;
        if (hour == 0) hour = 12;
        swprintf_s(buffer, L"%02d:%02d:%02d %ls",
            hour, time.wMinute, time.wSecond, time.wHour >= 12 ? L"PM" : L"AM");
        readings.Add(MetricTime, buffer, L"88:88:88 PM");
    }
    else
    {
        swprintf_s(buffer, L"%02d:%02d:%02d",
            time.wHour, time.wMinute, time.wSecond);
        readings.Add(MetricTime, buffer, L"88:88:88");
    }

    auto addInteger = [&](unsigned metric, unsigned bit,
                          int value, const wchar_t* suffix,
                          const wchar_t* reserve)
    {
        if (!settings.HasDetail(metric, bit))
            return;
        std::wstring output = value >= 0 ? std::to_wstring(value) : L"--";
        if (value >= 0)
            output += suffix;
        readings.Add(metric, output, reserve);
    };

    addInteger(MetricCpu, 1, metrics.CpuPercent, L"%", L"888%");
    addInteger(MetricCpu, 2, cpuTemperature.Celsius, L"\u00B0C", L"888\u00B0C");
    addInteger(MetricCpu, 4, metrics.CpuFrequencyMhz, L"MHz", L"88888MHz");
    addInteger(MetricGpu, 1, gpuMetrics.UsagePercent, L"%", L"888%");
    addInteger(MetricGpu, 2, gpuMetrics.TemperatureC, L"\u00B0C", L"888\u00B0C");
    addInteger(MetricVram, 1, gpuMetrics.VramPercent, L"%", L"888%");
    addInteger(MetricRam, 1, metrics.RamPercent, L"%", L"888%");

    auto addMemory = [&](unsigned metric, std::uint64_t used,
                         std::uint64_t total)
    {
        if (!settings.HasDetail(metric, 2))
            return;
        if (!total)
        {
            readings.Add(metric, L"--", L"888.8/888.8GB");
            return;
        }
        swprintf_s(buffer, L"%.1f/%.1fGB",
            static_cast<double>(used) / 1024.0,
            static_cast<double>(total) / 1024.0);
        readings.Add(metric, buffer, L"888.8/888.8GB");
    };

    addMemory(MetricVram, gpuMetrics.UsedVramMiB(), gpuMetrics.TotalVramMiB());
    addMemory(MetricRam, metrics.RamUsedMiB, metrics.RamTotalMiB);
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

    const bool alphaMode = transparentRenderer.Ready();
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

        if (transparentRenderer.Ready())
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
            {
                const std::wstring value = readings.Value(metric);
                DrawMetric(OverlayReadings::Label(metric), value.c_str());
            }
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
