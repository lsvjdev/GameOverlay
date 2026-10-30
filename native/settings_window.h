// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <string>
#include <cwchar>
#include <algorithm>
#if __has_include("build_info.h")
#include "build_info.h"
#else
#define GAMEOVERLAY_VERSION L"local development"
#define GAMEOVERLAY_REVISION L"local"
#endif
#include <vector>
#include "settings_config.h"

class SettingsWindow
{
public:
    explicit SettingsWindow(AppSettings& settings) : settings_(settings) {}

    void Refresh()
    {
        if (window_)
            InvalidateRect(window_, nullptr, FALSE);
    }

    void Show(HWND overlay)
    {
        overlay_ = overlay;

        if (window_ && IsWindow(window_))
        {
            ShowWindow(window_, SW_RESTORE);
            SetForegroundWindow(window_);
            return;
        }

        WNDCLASSW cls{};
        cls.lpfnWndProc = WndProc;
        cls.hInstance = GetModuleHandleW(nullptr);
        cls.lpszClassName = L"GameOverlaySettings";
        cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);

        if (!RegisterClassW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return;

        window_ = CreateWindowExW(
            WS_EX_APPWINDOW, cls.lpszClassName, L"Game Overlay - Settings",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            CW_USEDEFAULT, CW_USEDEFAULT, 810, 540,
            nullptr, nullptr, cls.hInstance, this);

        if (window_)
        {
            BOOL dark = TRUE;
            DwmSetWindowAttribute(window_, 20, &dark, sizeof(dark));
            ShowWindow(window_, SW_SHOW);
            UpdateWindow(window_);
            SetForegroundWindow(window_);
        }
    }

private:
    static constexpr COLORREF Background = RGB(19, 20, 24);
    static constexpr COLORREF Panel = RGB(29, 31, 36);
    static constexpr COLORREF Sidebar = RGB(25, 27, 32);
    static constexpr COLORREF Primary = RGB(235, 237, 243);
    static constexpr COLORREF Secondary = RGB(145, 150, 162);
    static constexpr COLORREF Accent = RGB(82, 217, 105);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        SettingsWindow* self = reinterpret_cast<SettingsWindow*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));

        if (msg == WM_NCCREATE)
        {
            auto create = reinterpret_cast<CREATESTRUCTW*>(lp);
            self = static_cast<SettingsWindow*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }

        return self ? self->Handle(hwnd, msg, wp, lp)
                    : DefWindowProcW(hwnd, msg, wp, lp);
    }

    static void Fill(HDC dc, RECT rect, COLORREF color)
    {
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(dc, &rect, brush);
        DeleteObject(brush);
    }

    static void Rounded(HDC dc, RECT rect, COLORREF color)
    {
        HBRUSH brush = CreateSolidBrush(color);
        HPEN pen = CreatePen(PS_SOLID, 1, color);
        auto oldBrush = SelectObject(dc, brush);
        auto oldPen = SelectObject(dc, pen);
        RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, 12, 12);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(pen);
        DeleteObject(brush);
    }

    void Text(HDC dc, const wchar_t* text, int x, int y, int width,
              HFONT font, COLORREF color)
    {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, color);
        auto previous = SelectObject(dc, font);
        RECT rect{x, y, x + width, y + 31};
        DrawTextW(dc, text, -1, &rect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(dc, previous);
    }

    void TextCenter(HDC dc, const wchar_t* text, int x, int y, int width,
                    HFONT font, COLORREF color)
    {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, color);
        auto previous = SelectObject(dc, font);
        RECT rect{x, y, x + width, y + 31};
        DrawTextW(dc, text, -1, &rect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, previous);
    }

    void Checkmark(HDC dc, int x, int y)
    {
        HPEN pen = CreatePen(PS_SOLID, 2, Background);
        auto old = SelectObject(dc, pen);
        MoveToEx(dc, x, y + 6, nullptr);
        LineTo(dc, x + 5, y + 11);
        LineTo(dc, x + 13, y + 2);
        SelectObject(dc, old);
        DeleteObject(pen);
    }

    void Arrow(HDC dc, int x, int y, bool up, COLORREF color)
    {
        HPEN pen = CreatePen(PS_SOLID, 2, color);
        auto old = SelectObject(dc, pen);
        const int d = up ? -1 : 1;
        MoveToEx(dc, x - 5, y - 3 * d, nullptr);
        LineTo(dc, x, y + 2 * d);
        LineTo(dc, x + 5, y - 3 * d);
        SelectObject(dc, old);
        DeleteObject(pen);
    }

    void Row(HDC dc, int y, const wchar_t* title,
             const wchar_t* description, const wchar_t* value, int right,
             int zone, bool toggle = false, bool on = false)
    {
        const COLORREF card = hoverZone_ == zone
            ? RGB(37, 40, 45) : Panel;
        Rounded(dc, {212, y, right, y + 72}, card);
        Text(dc, title, 231, y + 9, 340, medium_, Primary);
        Text(dc, description, 231, y + 38, right - 395, small_, Secondary);

        if (toggle)
        {
            const int sx = right - 83;
            const int sy = y + 21;
            Rounded(dc, {sx, sy, sx + 54, sy + 30},
                    on ? Accent : RGB(73, 78, 88));

            HBRUSH brush = CreateSolidBrush(on ? Background : Primary);
            auto previous = SelectObject(dc, brush);
            HPEN pen = CreatePen(PS_SOLID, 1, on ? Background : Primary);
            auto previousPen = SelectObject(dc, pen);
            const int circleX = on ? sx + 30 : sx + 5;
            Ellipse(dc, circleX, sy + 4, circleX + 21, sy + 25);
            SelectObject(dc, previousPen);
            SelectObject(dc, previous);
            DeleteObject(pen);
            DeleteObject(brush);
        }
        else
        {
            Text(dc, value, right - 167, y + 19, 146, medium_, Accent);
        }
    }

    static const wchar_t* MetricName(unsigned metric)
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
        return L"Unknown";
    }

    bool IsOpacitySlider(int x, int y, int right) const
    {
        return page_ == 2 && !settings_.transparent &&
            y >= 340 && y <= 396 &&
            x >= right - 270 && x <= right - 110;
    }

    void SetOpacityFromPointer(int x, int right, bool save)
    {
        const int left = right - 260, end = right - 115;
        const int value = (std::max)(0, (std::min)(100,
            (x - left) * 100 / (end - left)));
        if (settings_.opacity != value)
        {
            settings_.opacity = value;
            Update(save);
        }
        else if (save)
            settings_.Save();
    }

    static LRESULT CALLBACK OpacityEditorProc(HWND hwnd, UINT message,
                                               WPARAM wp, LPARAM lp)
    {
        auto* self = reinterpret_cast<SettingsWindow*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (!self)
            return DefWindowProcW(hwnd, message, wp, lp);

        if (message == WM_KEYDOWN)
        {
            if (wp == VK_RETURN)
            {
                self->FinishOpacityEdit(true);
                return 0;
            }
            if (wp == VK_ESCAPE)
            {
                self->FinishOpacityEdit(false);
                return 0;
            }
        }
        return CallWindowProcW(self->originalEditProc_, hwnd, message, wp, lp);
    }

    void BeginOpacityEdit(int right)
    {
        if (opacityEdit_)
            return;
        opacityEdit_ = CreateWindowExW(
            0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_CENTER |
                ES_AUTOHSCROLL,
            right - 99, 352, 78, 32, window_, nullptr,
            GetModuleHandleW(nullptr), nullptr);
        if (!opacityEdit_)
            return;
        SetWindowLongPtrW(opacityEdit_, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(this));
        originalEditProc_ = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(opacityEdit_, GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(OpacityEditorProc)));
        SendMessageW(opacityEdit_, WM_SETFONT,
                     reinterpret_cast<WPARAM>(medium_), TRUE);
        SetWindowTextW(opacityEdit_, std::to_wstring(settings_.opacity).c_str());
        SetFocus(opacityEdit_);
        SendMessageW(opacityEdit_, EM_SETSEL, 0, -1);
    }

    void FinishOpacityEdit(bool apply)
    {
        if (!opacityEdit_)
            return;
        wchar_t value[16]{};
        GetWindowTextW(opacityEdit_, value, 16);
        if (apply && value[0])
        {
            wchar_t* end = nullptr;
            const long entered = std::wcstol(value, &end, 10);
            if (end != value && *end == L'\0')
                settings_.opacity = static_cast<int>(
                    (std::max)(0L, (std::min)(100L, entered)));
        }
        HWND edit = opacityEdit_;
        opacityEdit_ = nullptr;
        DestroyWindow(edit);
        originalEditProc_ = nullptr;
        Update();
    }

    std::wstring LicenseFolder() const
    {
        wchar_t executable[MAX_PATH]{};
        const DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
        if (!length || length >= MAX_PATH)
            return {};
        const std::wstring path(executable, length);
        const auto slash = path.find_last_of(L"\\/");
        if (slash == std::wstring::npos)
            return {};
        return path.substr(0, slash + 1) + L"bin\\licenses";
    }

    void OpenLocalLicenses()
    {
        const auto path = LicenseFolder();
        const DWORD attributes = path.empty()
            ? INVALID_FILE_ATTRIBUTES : GetFileAttributesW(path.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY))
            ShellExecuteW(window_, L"open", path.c_str(),
                          nullptr, nullptr, SW_SHOWNORMAL);
        else
            MessageBoxW(window_,
                L"Local license files were not found. Extract the full portable ZIP.",
                L"Game Overlay", MB_OK | MB_ICONINFORMATION);
    }

    int Hotspot(int x, int y, int right) const
    {
        if (x >= 12 && x <= 178)
        {
            for (int i = 0; i < 4; ++i)
                if (y >= 121 + 56 * i && y < 164 + 56 * i)
                    return 100 + i;
        }

        if (x < 212 || x >= right)
            return -1;

        if (page_ == 1 && detailMetric_)
        {
            if (y >= 105 && y <= 138) return 600;
            const int index = AppSettings::MetricIndex(detailMetric_);
            const int count = (AppSettings::AllowedDetails(index) & 4) ? 3 : 2;
            for (int i = 0; i < count; ++i)
                if (y >= 141 + i * 94 && y < 213 + i * 94)
                    return 610 + i;
            return -1;
        }

        if (page_ == 0)
        {
            for (int i = 0; i < 3; ++i)
                if (y >= 141 + i * 94 && y < 213 + i * 94)
                    return 200 + i;
        }
        else if (page_ == 1)
        {
            for (int i = 0; i < 6; ++i)
                if (y >= 131 + i * 48 && y < 173 + i * 48)
                    return 300 + i;
        }
        else if (page_ == 2)
        {
            if (y >= 141 && y < 213) return 400;
            if (y >= 235 && y < 307) return 401;
            if (y >= 329 && y < 415 && !settings_.transparent &&
                x >= right - 270)
                return 402;
        }
        else if (page_ == 3)
        {
            for (int i = 0; i < 4; ++i)
                if (y >= 256 + i * 49 && y < 300 + i * 49)
                    return 500 + i;
        }
        return -1;
    }

    void Render(HDC dc, RECT area)
    {
        Fill(dc, area, Background);
        Fill(dc, {0, 0, 186, area.bottom}, Sidebar);

        Text(dc, L"Game Overlay", 23, 25, 160, brand_, Primary);
        Text(dc, L"native dev", 24, 57, 160, small_, Accent);

        const wchar_t* names[] = {L"General", L"Metrics", L"Appearance", L"About"};
        for (int i = 0; i < 4; ++i)
        {
            const int y = 121 + i * 56;
            if (page_ == i)
                Rounded(dc, {12, y, 173, y + 43}, RGB(39, 53, 44));
            else if (hoverZone_ == 100 + i)
                Rounded(dc, {12, y, 173, y + 43}, RGB(34, 36, 42));
            Text(dc, names[i], 29, y + 6, 142,
                 medium_, page_ == i ? Accent : Secondary);
        }

        const wchar_t* headings[] = {
            L"General", L"Displayed metrics", L"Appearance", L"About"
        };
        const wchar_t* captions[] = {
            L"Choose when the overlay appears",
            L"Select the readings you want to see",
            L"Customize your in-game display",
            L"Project details and licenses"
        };

        const wchar_t* detailTitle = detailMetric_ == MetricCpu ? L"CPU details" :
            detailMetric_ == MetricGpu ? L"GPU details" :
            detailMetric_ == MetricRam ? L"RAM details" : L"VRAM details";
        Text(dc, page_ == 1 && detailMetric_ ? detailTitle : headings[page_],
             215, 27, 490, heading_, Primary);
        Text(dc, page_ == 1 && detailMetric_
             ? L"Choose which readings appear in the overlay"
             : captions[page_], 216, 63, 530, small_, Secondary);

        const int right = area.right - 25;

        if (page_ == 0)
        {
            Row(dc, 141, L"Overlay enabled",
                L"Show or hide the performance strip",
                nullptr, right, 200, true, settings_.enabled);
            Row(dc, 235, L"Only while gaming",
                L"Hide in browsers, music apps and on the desktop",
                nullptr, right, 201, true, settings_.gameOnly);
            Row(dc, 329, L"12-hour clock",
                L"Toggle 12-hour or 24-hour time",
                nullptr, right, 202, true, settings_.clock12);
        }
        else if (page_ == 1)
        {
            if (detailMetric_)
            {
                Text(dc, L"< Back to metrics", 215, 102, 220, medium_, Accent);
                const wchar_t* titles[3]{};
                const wchar_t* descriptions[3]{};
                int count = 2;
                if (detailMetric_ == MetricCpu)
                {
                    titles[0] = L"CPU usage"; descriptions[0] = L"Processor utilization (%)";
                    titles[1] = L"CPU temperature"; descriptions[1] = L"Processor temperature (\u00B0C)";
                    titles[2] = L"CPU frequency"; descriptions[2] = L"Processor clock speed (MHz)";
                    count = 3;
                }
                else if (detailMetric_ == MetricGpu)
                {
                    titles[0] = L"GPU usage"; descriptions[0] = L"Graphics processor utilization (%)";
                    titles[1] = L"GPU temperature"; descriptions[1] = L"Graphics processor temperature (\u00B0C)";
                }
                else
                {
                    titles[0] = L"Memory usage"; descriptions[0] = L"Used memory as a percentage";
                    titles[1] = L"Used / total"; descriptions[1] = L"Memory amount in GB";
                }

                for (int i = 0; i < count; ++i)
                    Row(dc, 141 + i * 94, titles[i], descriptions[i],
                        nullptr, right, 610 + i, true,
                        settings_.HasDetail(detailMetric_, 1u << i));

                Text(dc, L"Keep at least one reading enabled. Hide the entire metric on the previous page.",
                     214, 419, right - 230, small_, Secondary);
            }
            else
            {
                for (int i = 0; i < 6; ++i)
                {
                    const int y = 131 + i * 48;
                    const unsigned metric = settings_.order[i];
                    const wchar_t* name = MetricName(metric);

                    Rounded(dc, {212, y, right, y + 42},
                        hoverZone_ == 300 + i ? RGB(37, 40, 45) : Panel);

                    const bool enabled = settings_.Has(metric);
                    Rounded(dc, {231, y + 10, 254, y + 33},
                        enabled ? Accent : RGB(68, 73, 81));
                    if (enabled)
                        Checkmark(dc, 236, y + 14);

                    Text(dc, name, 275, y + 5, right - 510, medium_,
                        enabled ? Primary : Secondary);
                    if (metric == MetricCpu || metric == MetricGpu ||
                        metric == MetricVram || metric == MetricRam)
                        Text(dc, L"EDIT >", right - 165, y + 5, 58,
                             small_, Accent);

                    Rounded(dc, {right - 100, y + 6, right - 61, y + 36},
                        i == 0 ? RGB(32, 34, 38) : RGB(47, 50, 57));
                    Rounded(dc, {right - 52, y + 6, right - 13, y + 36},
                        i == 5 ? RGB(32, 34, 38) : RGB(47, 50, 57));
                    Arrow(dc, right - 81, y + 21, true,
                          i == 0 ? Secondary : Accent);
                    Arrow(dc, right - 33, y + 21, false,
                          i == 5 ? Secondary : Accent);
                }

                Text(dc, L"Checkbox: show/hide. Name: edit readings. Arrows: reorder.",
                     214, 424, right - 230, small_, Secondary);
            }
        }
        else if (page_ == 2)
        {
            const wchar_t* locations[] = {
                L"Top left", L"Top center", L"Top right",
                L"Bottom left", L"Bottom center", L"Bottom right"
            };
            Row(dc, 141, L"Position", L"Click to cycle through positions",
                locations[settings_.position], right, 400);
            Row(dc, 235, L"Display style",
                L"Dark strip or transparent text",
                settings_.transparent ? L"Transparent" : L"Dark strip", right, 401);
            Rounded(dc, {212, 329, right, 415},
                settings_.transparent ? RGB(28, 29, 33) :
                (hoverZone_ == 402 ? RGB(37, 40, 45) : Panel));

            Text(dc, L"Background opacity", 231, 338, 260, medium_,
                 settings_.transparent ? Secondary : Primary);
            Text(dc, settings_.transparent
                 ? L"Not used for transparent text"
                 : L"Text remains fully visible",
                 231, 371, 270, small_, Secondary);

            if (settings_.transparent)
                TextCenter(dc, L"N/A", right - 105, 351, 78, medium_, Secondary);
            else
            {
                const int startX = right - 260;
                const int endX = right - 115;
                const int middleY = 366;
                Rounded(dc, {startX, middleY - 4, endX, middleY + 4},
                        RGB(63, 67, 74));
                const int pointX = startX + (endX - startX) * settings_.opacity / 100;
                if (pointX > startX)
                    Rounded(dc, {startX, middleY - 4, pointX, middleY + 4}, Accent);
                HBRUSH knob = CreateSolidBrush(Accent);
                HPEN pen = CreatePen(PS_SOLID, 1, Accent);
                auto oldBrush = SelectObject(dc, knob);
                auto oldPen = SelectObject(dc, pen);
                Ellipse(dc, pointX - 7, middleY - 7, pointX + 8, middleY + 8);
                SelectObject(dc, oldPen);
                SelectObject(dc, oldBrush);
                DeleteObject(pen);
                DeleteObject(knob);

                if (!opacityEdit_)
                {
                    Rounded(dc, {right - 101, 345, right - 19, 390},
                            RGB(47, 50, 57));
                    const auto percentage = std::to_wstring(settings_.opacity) + L"%";
                    TextCenter(dc, percentage.c_str(), right - 101, 351, 82,
                               medium_, Primary);
                }
            }

        }
        else
        {
            Rounded(dc, {212, 131, right, 244}, Panel);
            Text(dc, L"Game Overlay", 231, 142, 310, heading_, Primary);
            Text(dc, L"Native Windows overlay  |  GPL-3.0-only", 231, 178, 390,
                 small_, Secondary);
            const std::wstring version = std::wstring(L"Version: ") + GAMEOVERLAY_VERSION;
            const std::wstring revision = std::wstring(L"Revision: ") + GAMEOVERLAY_REVISION;
            Text(dc, version.c_str(), 231, 207, 320, small_, Accent);
            Text(dc, revision.c_str(), right - 202, 207, 181, small_, Secondary);

            const wchar_t* labels[] = {
                L"GitHub repository", L"Official website",
                L"Local licenses & credits", L"Report an issue"
            };
            const wchar_t* notes[] = {
                L"Source code and updates", L"Guides, changelog and documentation",
                L"View bundled legal notices", L"Report bugs and suggest improvements"
            };
            for (int i = 0; i < 4; ++i)
            {
                const int y = 256 + i * 49;
                Rounded(dc, {212, y, right, y + 44},
                    hoverZone_ == 500 + i ? RGB(37, 40, 45) : Panel);
                Text(dc, labels[i], 231, y + 2, 320, medium_, Primary);
                Text(dc, notes[i], 231, y + 23, 370, small_, Secondary);
                Text(dc, L"OPEN >", right - 79, y + 9, 65, small_, Accent);
            }
        }

        Text(dc, L"No telemetry or performance history is stored.",
             214, area.bottom - 43, area.right - 235, small_, Secondary);
    }

    void Update(bool persist = true)
    {
        if (persist)
            settings_.Save();
        if (overlay_ && IsWindow(overlay_))
            PostMessageW(overlay_, WM_APP + 2, 0, 0);
        InvalidateRect(window_, nullptr, FALSE);
    }

    void Click(int x, int y, int right)
    {
        if (x >= 12 && x <= 178)
        {
            for (int i = 0; i < 4; ++i)
                if (y >= 121 + 56 * i && y < 164 + 56 * i)
                {
                    if (opacityEdit_)
                        FinishOpacityEdit(true);
                    page_ = i;
                    detailMetric_ = 0;
                    InvalidateRect(window_, nullptr, TRUE);
                    return;
                }
        }

        if (x < 212 || x >= right)
            return;

        if (page_ == 0)
        {
            if (y >= 141 && y < 213) settings_.enabled = !settings_.enabled;
            else if (y >= 235 && y < 307) settings_.gameOnly = !settings_.gameOnly;
            else if (y >= 329 && y < 401) settings_.clock12 = !settings_.clock12;
            else return;
            Update();
        }
        else if (page_ == 1)
        {
            if (detailMetric_)
            {
                if (y >= 105 && y <= 138)
                {
                    detailMetric_ = 0;
                    InvalidateRect(window_, nullptr, TRUE);
                    return;
                }
                const int index = AppSettings::MetricIndex(detailMetric_);
                const int count = AppSettings::AllowedDetails(index) & 4 ? 3 : 2;
                for (int i = 0; i < count; ++i)
                    if (y >= 141 + i * 94 && y < 213 + i * 94)
                    {
                        if (settings_.ToggleDetail(detailMetric_, 1u << i))
                            Update();
                        return;
                    }
                return;
            }
            for (int i = 0; i < 6; ++i)
            {
                const int y0 = 131 + i * 48;
                if (y < y0 || y >= y0 + 42)
                    continue;
                const unsigned metric = settings_.order[i];
                if (x >= right - 100 && x < right - 61)
                {
                    if (MoveMetric(settings_.order, i, -1))
                        Update();
                }
                else if (x >= right - 52 && x < right - 13)
                {
                    if (MoveMetric(settings_.order, i, 1))
                        Update();
                }
                else if ((x >= 231 && x < 262) ||
                         metric == MetricFps || metric == MetricTime)
                {
                    settings_.metrics ^= metric;
                    Update();
                }
                else
                {
                    detailMetric_ = metric;
                    InvalidateRect(window_, nullptr, TRUE);
                }
                return;
            }
        }
        else if (page_ == 2)
        {
            if (y >= 141 && y < 213)
                settings_.position = (settings_.position + 1) % 6;
            else if (y >= 235 && y < 307)
                settings_.transparent = !settings_.transparent;
            else if (y >= 329 && y < 415 && !settings_.transparent &&
                     x >= right - 102 && x < right - 17)
            {
                BeginOpacityEdit(right);
                return;
            }
            else
                return;
            Update();
        }
        else if (page_ == 3)
        {
            if (y >= 256 && y < 300)
                ShellExecuteW(window_, L"open",
                    L"https://github.com/lsvjdev/GameOverlay",
                    nullptr, nullptr, SW_SHOWNORMAL);
            else if (y >= 305 && y < 349)
                ShellExecuteW(window_, L"open",
                    L"https://lsvjdev.github.io/GameOverlay/",
                    nullptr, nullptr, SW_SHOWNORMAL);
            else if (y >= 354 && y < 398)
                OpenLocalLicenses();
            else if (y >= 403 && y < 447)
                ShellExecuteW(window_, L"open",
                    L"https://github.com/lsvjdev/GameOverlay/issues/new/choose",
                    nullptr, nullptr, SW_SHOWNORMAL);
        }
    }

    LRESULT Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        switch (msg)
        {
        case WM_CREATE:
            brand_ = CreateFontW(-19, 0, 0, 0, FW_SEMIBOLD, FALSE,
                FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY,
                DEFAULT_PITCH, L"Segoe UI");
            heading_ = CreateFontW(-25, 0, 0, 0, FW_SEMIBOLD, FALSE,
                FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY,
                DEFAULT_PITCH, L"Segoe UI");
            medium_ = CreateFontW(-18, 0, 0, 0, FW_MEDIUM, FALSE,
                FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY,
                DEFAULT_PITCH, L"Segoe UI");
            small_ = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE,
                FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY,
                DEFAULT_PITCH, L"Segoe UI");
            editBrush_ = CreateSolidBrush(Panel);
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
            SetTextColor(reinterpret_cast<HDC>(wp), Primary);
            SetBkColor(reinterpret_cast<HDC>(wp), Panel);
            return reinterpret_cast<LRESULT>(editBrush_);

        case WM_PAINT:
        {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(hwnd, &paint);
            RECT rect{};
            GetClientRect(hwnd, &rect);

            const int width = rect.right - rect.left;
            const int height = rect.bottom - rect.top;
            HDC buffer = CreateCompatibleDC(dc);
            HBITMAP bitmap = buffer
                ? CreateCompatibleBitmap(dc, width, height) : nullptr;

            if (bitmap)
            {
                HGDIOBJ oldBitmap = SelectObject(buffer, bitmap);
                Render(buffer, rect);
                BitBlt(dc, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
                SelectObject(buffer, oldBitmap);
                DeleteObject(bitmap);
            }
            else
                Render(dc, rect);

            if (buffer)
                DeleteDC(buffer);

            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_LBUTTONDOWN:
        {
            RECT rect{};
            GetClientRect(hwnd, &rect);
            if (IsOpacitySlider(GET_X_LPARAM(lp), GET_Y_LPARAM(lp),
                                rect.right - 25))
            {
                if (opacityEdit_)
                    FinishOpacityEdit(true);
                draggingOpacity_ = true;
                SetCapture(hwnd);
                SetOpacityFromPointer(GET_X_LPARAM(lp), rect.right - 25, false);
                return 0;
            }
            return 0;
        }

        case WM_LBUTTONUP:
        {
            RECT rect{};
            GetClientRect(hwnd, &rect);
            if (draggingOpacity_)
            {
                draggingOpacity_ = false;
                ReleaseCapture();
                SetOpacityFromPointer(GET_X_LPARAM(lp), rect.right - 25, true);
                return 0;
            }
            Click(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), rect.right - 25);
            return 0;
        }

        case WM_CAPTURECHANGED:
            if (draggingOpacity_)
            {
                draggingOpacity_ = false;
                Update();
            }
            return 0;

        case WM_COMMAND:
            if (opacityEdit_ &&
                reinterpret_cast<HWND>(lp) == opacityEdit_ &&
                HIWORD(wp) == EN_KILLFOCUS)
                FinishOpacityEdit(true);
            return 0;

        case WM_MOUSEMOVE:
        {
            RECT rect{};
            GetClientRect(hwnd, &rect);
            if (draggingOpacity_)
            {
                SetOpacityFromPointer(GET_X_LPARAM(lp), rect.right - 25, false);
                return 0;
            }
            const int zone = Hotspot(GET_X_LPARAM(lp), GET_Y_LPARAM(lp),
                                      rect.right - 25);
            if (zone != hoverZone_)
            {
                hoverZone_ = zone;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            if (!trackingMouse_)
            {
                TRACKMOUSEEVENT track{};
                track.cbSize = sizeof(track);
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = hwnd;
                trackingMouse_ = TrackMouseEvent(&track) != FALSE;
            }
            return 0;
        }

        case WM_MOUSELEAVE:
            trackingMouse_ = false;
            hoverZone_ = -1;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT)
            {
                POINT mouse{};
                GetCursorPos(&mouse);
                ScreenToClient(hwnd, &mouse);
                RECT rect{};
                GetClientRect(hwnd, &rect);
                if (Hotspot(mouse.x, mouse.y, rect.right - 25) != -1)
                {
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                    return TRUE;
                }
            }
            break;

        case WM_SIZE:
            if (opacityEdit_)
            {
                RECT rect{};
                GetClientRect(hwnd, &rect);
                const int right = rect.right - 25;
                MoveWindow(opacityEdit_, right - 99, 352, 78, 32, TRUE);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_KEYDOWN:
            if (wp == VK_ESCAPE)
            {
                if (detailMetric_)
                {
                    detailMetric_ = 0;
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
                else
                    DestroyWindow(hwnd);
            }
            return 0;

        case WM_DESTROY:
            if (brand_) DeleteObject(brand_);
            if (heading_) DeleteObject(heading_);
            if (medium_) DeleteObject(medium_);
            if (small_) DeleteObject(small_);
            if (editBrush_) DeleteObject(editBrush_);
            brand_ = heading_ = medium_ = small_ = nullptr;
            editBrush_ = nullptr;
            opacityEdit_ = nullptr;
            window_ = nullptr;
            detailMetric_ = 0;
            return 0;
        }

        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    AppSettings& settings_;
    HWND window_ = nullptr;
    HWND overlay_ = nullptr;
    HWND opacityEdit_ = nullptr;
    WNDPROC originalEditProc_ = nullptr;
    HFONT brand_ = nullptr;
    HFONT heading_ = nullptr;
    HFONT medium_ = nullptr;
    HFONT small_ = nullptr;
    HBRUSH editBrush_ = nullptr;
    int page_ = 0;
    unsigned detailMetric_ = 0;
    bool draggingOpacity_ = false;
    int hoverZone_ = -1;
    bool trackingMouse_ = false;
};
