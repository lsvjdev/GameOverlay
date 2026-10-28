// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 LSVJ-Coding

using System;
using System.IO;
using System.Text.Json;

namespace LSVJOverlay;

internal enum StripPosition
{
    TopLeft,
    TopCenter,
    TopRight
}

internal sealed class AppSettings
{
    public bool Use24HourClock { get; set; } = true;
    public StripPosition DesktopPosition { get; set; } = StripPosition.TopLeft;

    private static string SettingsPath => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "GameOverlay", "settings.json");

    public static AppSettings Load()
    {
        try
        {
            if (File.Exists(SettingsPath))
            {
                return JsonSerializer.Deserialize<AppSettings>(File.ReadAllText(SettingsPath))
                    ?? new AppSettings();
            }
        }
        catch (Exception) { /* Keep defaults if settings are invalid or unavailable. */ }
        return new AppSettings();
    }

    public void Save()
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(SettingsPath)!);
            File.WriteAllText(SettingsPath, JsonSerializer.Serialize(this,
                new JsonSerializerOptions { WriteIndented = true }));
        }
        catch (Exception) { /* Settings cannot be saved in a restricted profile. */ }
    }
}
