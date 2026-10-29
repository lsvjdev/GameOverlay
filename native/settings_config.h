// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <windows.h>
#include <string>
#include "metric_order.h"

struct AppSettings
{
    bool enabled = true;
    bool gameOnly = false;
    bool clock12 = true;
    bool transparent = false;
    int opacity = 92;
    int position = 0;
    unsigned metrics = MetricFps | MetricTime | MetricCpu |
                       MetricGpu | MetricVram | MetricRam;
    std::array<unsigned, 6> order = DefaultMetricOrder;

    std::wstring Path() const
    {
        wchar_t appData[MAX_PATH]{};
        DWORD length = GetEnvironmentVariableW(
            L"LOCALAPPDATA", appData, MAX_PATH);
        if (!length || length >= MAX_PATH)
            return {};

        std::wstring folder(appData, length);
        folder += L"\\GameOverlay";
        CreateDirectoryW(folder.c_str(), nullptr);
        return folder + L"\\settings.ini";
    }

    void Load()
    {
        const auto path = Path();
        if (path.empty())
            return;

        auto read = [&](const wchar_t* key, int fallback)
        {
            return static_cast<int>(GetPrivateProfileIntW(
                L"overlay", key, fallback, path.c_str()));
        };

        enabled = read(L"enabled", 1) != 0;
        gameOnly = read(L"game_only", 0) != 0;
        clock12 = read(L"clock_12", 1) != 0;
        transparent = read(L"transparent", 0) != 0;
        opacity = read(L"opacity", 92);
        position = read(L"position", 0);
        metrics = static_cast<unsigned>(read(L"metrics", 63));

        for (int i = 0; i < 6; ++i)
        {
            const auto key = L"order_" + std::to_wstring(i);
            order[i] = static_cast<unsigned>(read(key.c_str(),
                                                  DefaultMetricOrder[i]));
        }

        if (!IsValidMetricOrder(order))
            order = DefaultMetricOrder;

        if (opacity < 0 || opacity > 100)
            opacity = 92;
        if (position < 0 || position > 5)
            position = 0;
        metrics &= 63;
    }

    void Save() const
    {
        const auto path = Path();
        if (path.empty())
            return;

        auto write = [&](const wchar_t* key, int value)
        {
            const auto text = std::to_wstring(value);
            WritePrivateProfileStringW(
                L"overlay", key, text.c_str(), path.c_str());
        };

        write(L"enabled", enabled);
        write(L"game_only", gameOnly);
        write(L"clock_12", clock12);
        write(L"transparent", transparent);
        write(L"opacity", opacity);
        write(L"position", position);
        write(L"metrics", static_cast<int>(metrics));

        for (int i = 0; i < 6; ++i)
        {
            const auto key = L"order_" + std::to_wstring(i);
            write(key.c_str(), static_cast<int>(order[i]));
        }
    }

    bool Has(unsigned metric) const
    {
        return (metrics & metric) != 0;
    }
};
