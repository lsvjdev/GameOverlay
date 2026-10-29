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
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 860, 650,
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

        if (page_ == 0)
        {
            for (int i = 0; i < 3; ++i)
                if (y >= 141 + i * 94 && y < 213 + i * 94)
                    return 200 + i;
        }
        else if (page_ == 1)
        {
            for (int i = 0; i < 6; ++i)
                if (y >= 139 + i * 54 && y < 187 + i * 54)
                    return 300 + i;
        }
        else if (page_ == 2)
        {
            if (y >= 141 && y < 213) return 400;
            if (y >= 235 && y < 307) return 401;
            if (y >= 329 && y < 415 && !settings_.transparent &&
                x >= right - 165)
                return 402;
        }
        else if (page_ == 3)
        {
            if (licenseIndex_ >= 0)
                return (y >= 106 && y < 146) ? 500 : -1;

            if (y >= 141 && y < 213) return 500;
            for (int i = 0; i < 4; ++i)
                if (y >= 230 + i * 56 && y < 278 + i * 56)
                    return 501 + i;
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

        Text(dc, headings[page_], 215, 27, 490, heading_, Primary);
        Text(dc, captions[page_], 216, 63, 530, small_, Secondary);

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
            for (int i = 0; i < 6; ++i)
            {
                const int y = 139 + i * 54;
                const unsigned metric = settings_.order[i];
                const wchar_t* name = L"Unknown";

                switch (metric)
                {
                case MetricFps: name = L"FPS"; break;
                case MetricTime: name = L"Time"; break;
                case MetricCpu: name = L"CPU"; break;
                case MetricGpu: name = L"GPU"; break;
                case MetricVram: name = L"VRAM"; break;
                case MetricRam: name = L"RAM"; break;
                }

                Rounded(dc, {212, y, right, y + 48},
                    hoverZone_ == 300 + i ? RGB(37, 40, 45) : Panel);

                const bool enabled = settings_.Has(metric);
                Rounded(dc, {231, y + 13, 254, y + 36},
                    enabled ? Accent : RGB(68, 73, 81));

                if (enabled)
                    Checkmark(dc, 236, y + 17);

                Text(dc, name, 275, y + 8, right - 400, medium_,
                     enabled ? Primary : Secondary);

                Rounded(dc, {right - 100, y + 9, right - 61, y + 39},
                    i == 0 ? RGB(32, 34, 38) : RGB(47, 50, 57));
                Rounded(dc, {right - 52, y + 9, right - 13, y + 39},
                    i == 5 ? RGB(32, 34, 38) : RGB(47, 50, 57));
                Arrow(dc, right - 81, y + 24, true,
                      i == 0 ? Secondary : Accent);
                Arrow(dc, right - 33, y + 24, false,
                      i == 5 ? Secondary : Accent);
            }

            Text(dc, L"Use arrows to reorder. Click a metric to show or hide it.",
                 214, 480, right - 230, small_, Secondary);
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

            Text(dc, L"Opacity", 231, 338, 240, medium_,
                 settings_.transparent ? Secondary : Primary);
            Text(dc, settings_.transparent
                 ? L"Not used for transparent text"
                 : L"Dark strip opacity (1% steps)",
                 231, 371, 320, small_, Secondary);

            if (!settings_.transparent)
            {
                Rounded(dc, {right - 163, 346, right - 128, 385},
                        RGB(47, 50, 57));
                Rounded(dc, {right - 51, 346, right - 16, 385},
                        RGB(47, 50, 57));
                Text(dc, L"-", right - 151, 352, 20, medium_, Accent);
                Text(dc, L"+", right - 41, 352, 20, medium_, Accent);

                const auto opacity = std::to_wstring(settings_.opacity) + L"%";
                Text(dc, opacity.c_str(), right - 115, 352, 63, medium_, Primary);
            }
            else
            {
                Text(dc, L"N/A", right - 95, 352, 70, medium_, Secondary);
            }

            Text(dc, L"Transparent mode keeps text fully opaque.",
                 214, 448, 530, small_, Secondary);
        }
        else if (licenseIndex_ < 0)
        {
            Row(dc, 141, L"GitHub repository",
                L"Source code, issues and updates",
                L"OPEN", right, 500);
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
            for (int i = 0; i < 6; ++i)
            {
                const int y0 = 139 + i * 54;
                if (y >= y0 && y < y0 + 48)
                {
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
                    else
                    {
                        settings_.metrics ^= settings_.order[i];
                        Update();
                    }
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
            else if (y >= 329 && y < 415 && !settings_.transparent)
            {
                if (x >= right - 163 && x <= right - 128)
                    settings_.opacity = settings_.opacity > 0
                        ? settings_.opacity - 1 : 0;
                else if (x >= right - 51 && x <= right - 16)
                    settings_.opacity = settings_.opacity < 100
                        ? settings_.opacity + 1 : 100;
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
        case WM_GETMINMAXINFO:
        {
            auto limits = reinterpret_cast<MINMAXINFO*>(lp);
            limits->ptMinTrackSize = {830, 620};
            return 0;
        }

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

        case WM_MOUSEMOVE:
        {
            RECT rect{};
            GetClientRect(hwnd, &rect);
            const int zone = Hotspot(GET_X_LPARAM(lp), GET_Y_LPARAM(lp),
                                      rect.right - 25);
            if (zone != hoverZone_)
            {
                hoverZone_ = zone;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            TRACKMOUSEEVENT track{};
            track.cbSize = sizeof(track);
            track.dwFlags = TME_LEAVE;
            track.hwndTrack = hwnd;
            TrackMouseEvent(&track);
            return 0;
        }

        case WM_MOUSELEAVE:
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
            if (brand_) DeleteObject(brand_);
            if (heading_) DeleteObject(heading_);
            if (medium_) DeleteObject(medium_);
            if (small_) DeleteObject(small_);
            if (editBrush_) DeleteObject(editBrush_);
            brand_ = heading_ = medium_ = small_ = nullptr;
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
    HFONT brand_ = nullptr;
    HFONT heading_ = nullptr;
    HFONT medium_ = nullptr;
    HFONT small_ = nullptr;
    HBRUSH editBrush_ = nullptr;
    int page_ = 0;
    int licenseIndex_ = -1;
    int hoverZone_ = -1;
};
