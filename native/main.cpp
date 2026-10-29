
 // SPDX-License-Identifier: GPL-3.0-only
 // Copyright (c) 2026 lsvjdev

 #include <windows.h>
 #include <cstdio>

 constexpr int ToggleHotkey = 1;
 constexpr int ExitHotkey = 2;

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

         wchar_t text[128];
         swprintf_s(text, L"GAME OVERLAY DEV   %02d:%02d:%02d",
                    time.wHour, time.wMinute, time.wSecond);

         HFONT font = CreateFontW(
             -16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
             DEFAULT_PITCH, L"Segoe UI");

         HGDIOBJ previous = SelectObject(dc, font);
         SetBkMode(dc, TRANSPARENT);
         SetTextColor(dc, RGB(82, 217, 85));

         area.left += 10;
         DrawTextW(dc, text, -1, &area,
                   DT_SINGLELINE | DT_VCENTER | DT_LEFT);

         SelectObject(dc, previous);
         DeleteObject(font);
         EndPaint(window, &paint);
         return 0;
     }

     case WM_TIMER:
         InvalidateRect(window, nullptr, FALSE);
         return 0;

     case WM_HOTKEY:
         if (wp == ToggleHotkey)
             ShowWindow(window, IsWindowVisible(window)
                         ? SW_HIDE : SW_SHOWNOACTIVATE);
         else if (wp == ExitHotkey)
             DestroyWindow(window);
         return 0;

     case WM_DESTROY:
         KillTimer(window, 1);
         UnregisterHotKey(window, ToggleHotkey);
         UnregisterHotKey(window, ExitHotkey);
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

     SetTimer(window, 1, 1000, nullptr);
     ShowWindow(window, SW_SHOWNOACTIVATE);

     MSG message;
     while (GetMessageW(&message, nullptr, 0, 0) > 0)
     {
         TranslateMessage(&message);
         DispatchMessageW(&message);
     }

     return 0;
 }
