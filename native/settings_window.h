// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <string>
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

    void Row(HDC dc, int y, const wchar_t* title,
             const wchar_t* description, const wchar_t* value, int right)
    {
        Rounded(dc, {212, y, right, y + 72}, Panel);
        Text(dc, title, 231, y + 9, 340, medium_, Primary);
        Text(dc, description, 231, y + 38, 390, small_, Secondary);
        Text(dc, value, right - 167, y + 19, 146, medium_, Accent);
    }

    void Render(HDC dc, RECT area)
    {
        Fill(dc, area, Background);
        Fill(dc, {0, 0, 186, area.bottom}, Sidebar);

        Text(dc, L"Game Overlay", 23, 25, 160, heading_, Primary);
        Text(dc, L"native dev", 24, 57, 160, small_, Accent);

        const wchar_t* names[] = {L"General", L"Metrics", L"Appearance", L"About"};
        for (int i = 0; i < 4; ++i)
        {
            const int y = 121 + i * 56;
            if (page_ == i)
                Rounded(dc, {12, y, 173, y + 43}, RGB(39, 53, 44));
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

        Text(dc, headings[page_], 215, 27, 490, heading_, Primary);
        Text(dc, captions[page_], 216, 63, 530, small_, Secondary);

        const int right = area.right - 25;

        if (page_ == 0)
        {
            Row(dc, 141, L"Overlay enabled",
                L"Show or hide the performance strip",
                settings_.enabled ? L"ON" : L"OFF", right);
            Row(dc, 235, L"Only while gaming",
                L"Hide unless the foreground game is reporting FPS",
                settings_.gameOnly ? L"ON" : L"OFF", right);
            Row(dc, 329, L"12-hour clock",
                L"Toggle 12-hour or 24-hour time",
                settings_.clock12 ? L"12-hour" : L"24-hour", right);
        }
        else if (page_ == 1)
        {
            const wchar_t* labels[] = {L"FPS", L"Time", L"CPU", L"GPU", L"VRAM", L"RAM"};
            const unsigned bits[] = {
                MetricFps, MetricTime, MetricCpu, MetricGpu, MetricVram, MetricRam
            };
            const int columnWidth = (right - 224) / 2;

            for (int i = 0; i < 6; ++i)
            {
                const int x = 212 + (i % 2) * (columnWidth + 10);
                const int y = 141 + (i / 2) * 94;
                Rounded(dc, {x, y, x + columnWidth, y + 72}, Panel);
                Rounded(dc, {x + 19, y + 22, x + 42, y + 45},
                    settings_.Has(bits[i]) ? Accent : RGB(67, 72, 81));
                if (settings_.Has(bits[i]))
                    Text(dc, L"\\u2713", x + 23, y + 16, 30, medium_, Background);
                Text(dc, labels[i], x + 58, y + 18, columnWidth - 70,
                     medium_, Primary);
            }
            Text(dc, L"Changes appear instantly in the overlay.",
                 214, 443, 510, small_, Secondary);
        }
        else if (page_ == 2)
        {
            const wchar_t* locations[] = {
                L"Top left", L"Top center", L"Top right",
                L"Bottom left", L"Bottom center", L"Bottom right"
            };
            Row(dc, 141, L"Position", L"Click to cycle through positions",
                locations[settings_.position], right);
            Row(dc, 235, L"Display style",
                L"Dark strip or transparent text",
                settings_.transparent ? L"Transparent" : L"Dark strip", right);
            Rounded(dc, {212, 329, right, 401}, Panel);
            Text(dc, L"Opacity", 231, 337, 250, medium_, Primary);
            Text(dc, L"Dark strip background transparency",
                 231, 366, 300, small_, Secondary);
            Text(dc, L"-", right - 147, 346, 25, heading_, Accent);
            const auto opacity = std::to_wstring(settings_.opacity) + L"%";
            Text(dc, opacity.c_str(), right - 115, 348, 60, medium_, Primary);
            Text(dc, L"+", right - 35, 346, 25, heading_, Accent);
            Text(dc, L"Tip: the transparent style uses text without the dark bar.",
                 214, 443, 530, small_, Secondary);
        }
        else if (licenseIndex_ < 0)
        {
            Row(dc, 141, L"GitHub repository",
                L"Source code, issues and updates",
                L"OPEN", right);
            const wchar_t* labels[] = {
                L"Game Overlay license", L"PresentMon license",
                L"PawnIO module license", L"PawnIO module sources"
            };
            for (int i = 0; i < 4; ++i)
            {
                const int y = 230 + i * 56;
                Rounded(dc, {212, y, right, y + 48}, Panel);
                Text(dc, labels[i], 231, y + 8, 430, medium_, Primary);
                Text(dc, L"VIEW", right - 65, y + 8, 53, small_, Accent);
            }
        }
        else
        {
            Text(dc, L"< Back to About", 216, 111, 320, medium_, Accent);
        }

        Text(dc, L"No telemetry or performance history is stored.",
             214, area.bottom - 43, area.right - 235, small_, Secondary);
    }

    std::wstring LicensePath(int index) const
    {
        const wchar_t* names[] = {
            L"LICENSE",
            L"PresentMon-LICENSE.txt",
            L"PawnIO-Modules-LICENSE.txt",
            L"PawnIO-Modules-SOURCE.txt"
        };
        if (index < 0 || index >= 4)
            return {};

        wchar_t executable[MAX_PATH]{};
        const DWORD len = GetModuleFileNameW(nullptr, executable, MAX_PATH);
        if (!len || len >= MAX_PATH)
            return {};

        const std::wstring path(executable, len);
        const size_t slash = path.find_last_of(L"\\/");
        if (slash == std::wstring::npos)
            return {};

        return path.substr(0, slash + 1) + L"bin\\licenses\\" + names[index];
    }

    void ViewLicense(int index)
    {
        const auto path = LicensePath(index);
        std::wstring text;

        HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (file != INVALID_HANDLE_VALUE)
        {
            LARGE_INTEGER size{};
            if (GetFileSizeEx(file, &size) && size.QuadPart >= 0 &&
                size.QuadPart <= 1024 * 1024)
            {
                std::string bytes(static_cast<size_t>(size.QuadPart), '\0');
                DWORD received = 0;
                if (ReadFile(file, bytes.data(),
                             static_cast<DWORD>(bytes.size()), &received, nullptr))
                {
                    int count = MultiByteToWideChar(
                        CP_UTF8, 0, bytes.data(), received, nullptr, 0);
                    if (count > 0)
                    {
                        text.resize(count);
                        MultiByteToWideChar(
                            CP_UTF8, 0, bytes.data(), received,
                            text.data(), count);
                    }
                }
            }
            CloseHandle(file);
        }

        if (text.empty())
            text = L"License file is unavailable.\r\n\r\n"
                   L"Please extract the complete Game Overlay ZIP.";

        licenseIndex_ = index;
        if (!licenseEdit_)
        {
            licenseEdit_ = CreateWindowExW(
                0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                ES_AUTOVSCROLL | ES_READONLY,
                214, 153, 540, 300, window_,
                nullptr, GetModuleHandleW(nullptr), nullptr);
            SendMessageW(licenseEdit_, WM_SETFONT,
                reinterpret_cast<WPARAM>(small_), TRUE);
        }

        SetWindowTextW(licenseEdit_, text.c_str());
        RECT area{};
        GetClientRect(window_, &area);
        MoveWindow(licenseEdit_, 214, 154,
            area.right - 240, area.bottom - 214, TRUE);
        ShowWindow(licenseEdit_, SW_SHOW);
        InvalidateRect(window_, nullptr, TRUE);
    }

    void Update()
    {
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
                    page_ = i;
                    licenseIndex_ = -1;
                    if (licenseEdit_)
                        ShowWindow(licenseEdit_, SW_HIDE);
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
            const unsigned bits[] = {
                MetricFps, MetricTime, MetricCpu, MetricGpu, MetricVram, MetricRam
            };
            const int colWidth = (right - 224) / 2;
            for (int i = 0; i < 6; ++i)
            {
                const int left = 212 + (i % 2) * (colWidth + 10);
                const int top = 141 + (i / 2) * 94;
                if (x >= left && x < left + colWidth &&
                    y >= top && y < top + 72)
                {
                    settings_.metrics ^= bits[i];
                    Update();
                    return;
                }
            }
        }
        else if (page_ == 2)
        {
            if (y >= 141 && y < 213)
                settings_.position = (settings_.position + 1) % 6;
            else if (y >= 235 && y < 307)
                settings_.transparent = !settings_.transparent;
            else if (y >= 329 && y < 401)
            {
                if (x >= right - 160 && x <= right - 115)
                    settings_.opacity = (settings_.opacity > 25)
                        ? settings_.opacity - 5 : 25;
                else if (x >= right - 50)
                    settings_.opacity = (settings_.opacity < 100)
                        ? settings_.opacity + 5 : 100;
                else
                    return;
            }
            else return;
            Update();
        }
        else if (page_ == 3)
        {
            if (licenseIndex_ >= 0)
            {
                if (y >= 106 && y < 146)
                {
                    licenseIndex_ = -1;
                    ShowWindow(licenseEdit_, SW_HIDE);
                    InvalidateRect(window_, nullptr, TRUE);
                }
                return;
            }

            if (y >= 141 && y < 213)
                ShellExecuteW(window_, L"open",
                    L"https://github.com/lsvjdev/GameOverlay",
                    nullptr, nullptr, SW_SHOWNORMAL);
            else
                for (int i = 0; i < 4; ++i)
                    if (y >= 230 + 56 * i && y < 278 + 56 * i)
                    {
                        ViewLicense(i);
                        break;
                    }
        }
    }

    LRESULT Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        switch (msg)
        {
        case WM_CREATE:
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
            Render(dc, rect);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_LBUTTONUP:
        {
            RECT rect{};
            GetClientRect(hwnd, &rect);
            Click(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), rect.right - 25);
            return 0;
        }

        case WM_SIZE:
            if (licenseEdit_ && licenseIndex_ >= 0)
            {
                RECT rect{};
                GetClientRect(hwnd, &rect);
                MoveWindow(licenseEdit_, 214, 154,
                    rect.right - 240, rect.bottom - 214, TRUE);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_KEYDOWN:
            if (wp == VK_ESCAPE)
            {
                if (licenseIndex_ >= 0)
                {
                    licenseIndex_ = -1;
                    ShowWindow(licenseEdit_, SW_HIDE);
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
                else
                    DestroyWindow(hwnd);
            }
            return 0;

        case WM_DESTROY:
            if (heading_) DeleteObject(heading_);
            if (medium_) DeleteObject(medium_);
            if (small_) DeleteObject(small_);
            if (editBrush_) DeleteObject(editBrush_);
            heading_ = medium_ = small_ = nullptr;
            editBrush_ = nullptr;
            licenseEdit_ = nullptr;
            window_ = nullptr;
            licenseIndex_ = -1;
            return 0;
        }

        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    AppSettings& settings_;
    HWND window_ = nullptr;
    HWND overlay_ = nullptr;
    HWND licenseEdit_ = nullptr;
    HFONT heading_ = nullptr;
    HFONT medium_ = nullptr;
    HFONT small_ = nullptr;
    HBRUSH editBrush_ = nullptr;
    int page_ = 0;
    int licenseIndex_ = -1;
};
