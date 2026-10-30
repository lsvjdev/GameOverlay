// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <windows.h>
#include <gdiplus.h>
#include <array>
#include <cmath>
#include <algorithm>

class TransparentOverlayRenderer
{
public:
    bool Start()
    {
        if (token_)
            return true;

        Gdiplus::GdiplusStartupInput startup;
        return Gdiplus::GdiplusStartup(&token_, &startup, nullptr) ==
            Gdiplus::Ok;
    }

    bool Ready() const
    {
        return token_ != 0;
    }

    void Stop()
    {
        if (token_)
        {
            Gdiplus::GdiplusShutdown(token_);
            token_ = 0;
        }
    }

    bool Render(HWND window, const AppSettings& settings,
                const OverlayReadings& readings)
    {
        if (!Ready())
            return false;

        Gdiplus::FontFamily family(L"Segoe UI");
        Gdiplus::Font regular(&family, 14.0f,
            Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::Font bold(&family, 14.0f,
            Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush labelBrush(settings.transparent
            ? Gdiplus::Color(255, 247, 248, 250)
            : Gdiplus::Color(255, 189, 181, 188));
        Gdiplus::SolidBrush valueBrush(settings.transparent
            ? Gdiplus::Color(255, 255, 255, 255)
            : Gdiplus::Color(255, 82, 217, 85));
        Gdiplus::SolidBrush separatorBrush(Gdiplus::Color(255, 210, 214, 219));
        Gdiplus::SolidBrush shadowBrush(Gdiplus::Color(170, 0, 0, 0));

        Gdiplus::StringFormat format;
        format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap |
                              Gdiplus::StringFormatFlagsMeasureTrailingSpaces);

        Gdiplus::Bitmap measuringSurface(1, 1, PixelFormat32bppPARGB);
        Gdiplus::Graphics measuring(&measuringSurface);

        struct Part
        {
            const wchar_t* text = nullptr;
            Gdiplus::Font* font = nullptr;
            Gdiplus::SolidBrush* brush = nullptr;
            float x = 0;
            bool separator = false;
        };

        std::array<Part, 32> parts{};
        int count = 0;
        float x = 7.0f;

        auto measureWidth = [&](const wchar_t* text, Gdiplus::Font* font)
        {
            Gdiplus::RectF bounds;
            measuring.MeasureString(text, -1, font,
                Gdiplus::PointF(0.0f, 0.0f), &format, &bounds);
            return bounds.Width;
        };

        auto append = [&](const wchar_t* text, const wchar_t* reserve,
                          Gdiplus::Font* font, Gdiplus::SolidBrush* brush)
        {
            if (count >= static_cast<int>(parts.size()))
                return;
            parts[count++] = {text, font, brush, std::round(x), false};
            // Rounded pixel positions keep each character's baseline stable,
            // while static reservation prevents metric-width movement.
            x += std::ceil((std::max)(measureWidth(text, font),
                                     measureWidth(reserve, font)));
        };

        auto appendSeparator = [&]()
        {
            if (count >= static_cast<int>(parts.size()))
                return;
            // Use a fixed pixel rule instead of a resampled "|" glyph.
            parts[count++] = {nullptr, nullptr, &separatorBrush,
                              std::round(x), true};
            x += 1.0f;
        };

        for (unsigned metric : settings.order)
        {
            if (!settings.Has(metric))
                continue;

            if (count)
            {
                if (settings.transparent)
                {
                    x += 9.0f;
                    appendSeparator();
                    x += 9.0f;
                }
                else
                    x += 16.0f;
            }

            const auto label = OverlayReadings::Label(metric);
            append(label, label, &regular, &labelBrush);

            const auto& value = readings.Get(metric);
            for (int i = 0; i < value.count; ++i)
            {
                x += i == 0 ? 4.0f : 5.0f;
                const auto& part = value.parts[i];
                append(part.value.c_str(), part.reserve.c_str(),
                    &bold, &valueBrush);
            }
        }

        const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        if (screenWidth <= 0 || screenHeight <= 0)
            return false;

        const int height = settings.transparent ? 27 : 28;
        const int contentWidth = count ? static_cast<int>(std::ceil(x)) + 8 : 100;
        const int width = contentWidth > screenWidth
            ? screenWidth : contentWidth;

        const int horizontal = settings.position % 3;
        const int targetX = horizontal == 0 ? 0 :
            (horizontal == 1 ? (screenWidth - width) / 2 :
                               screenWidth - width);
        const int targetY = settings.position < 3
            ? 0 : screenHeight - height;

        HDC screen = GetDC(nullptr);
        if (!screen)
            return false;

        HDC memory = CreateCompatibleDC(screen);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        void* pixels = nullptr;
        HBITMAP bitmap = memory
            ? CreateDIBSection(screen, &info, DIB_RGB_COLORS,
                               &pixels, nullptr, 0)
            : nullptr;

        if (!bitmap || !pixels)
        {
            if (bitmap)
                DeleteObject(bitmap);
            if (memory)
                DeleteDC(memory);
            ReleaseDC(nullptr, screen);
            return false;
        }

        HGDIOBJ previous = SelectObject(memory, bitmap);
        {
            Gdiplus::Bitmap surface(width, height, width * 4,
                PixelFormat32bppPARGB, static_cast<BYTE*>(pixels));
            Gdiplus::Graphics painter(&surface);
            painter.Clear(Gdiplus::Color(0, 0, 0, 0));
            if (!settings.transparent)
            {
                const int percentage = (std::max)(0, (std::min)(100, settings.opacity));
                Gdiplus::SolidBrush background(Gdiplus::Color(
                    static_cast<BYTE>(percentage * 255 / 100), 24, 25, 28));
                painter.FillRectangle(&background, 0, 0, width, height);
            }
            painter.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

            for (int i = 0; i < count; ++i)
            {
                const Part& part = parts[i];
                if (part.separator)
                {
                    if (settings.transparent)
                        painter.FillRectangle(&shadowBrush,
                            static_cast<int>(part.x) + 1, 7, 1, 13);
                    painter.FillRectangle(&separatorBrush,
                        static_cast<int>(part.x), 7, 1, 13);
                }
                else
                {
                    if (settings.transparent)
                        painter.DrawString(part.text, -1, part.font,
                            Gdiplus::PointF(part.x + 1.0f, 5.0f),
                            &format, &shadowBrush);
                    painter.DrawString(part.text, -1, part.font,
                        Gdiplus::PointF(part.x, 4.0f), &format, part.brush);
                }
            }
            painter.Flush();
        }

        POINT position{targetX, targetY};
        POINT source{0, 0};
        SIZE size{width, height};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

        const BOOL result = UpdateLayeredWindow(
            window, screen, &position, &size, memory,
            &source, 0, &blend, ULW_ALPHA);

        SelectObject(memory, previous);
        DeleteObject(bitmap);
        DeleteDC(memory);
        ReleaseDC(nullptr, screen);
        return result != FALSE;
    }

private:
    ULONG_PTR token_ = 0;
};
