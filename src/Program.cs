// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 lsvjdev

using System;
using System.Windows.Forms;

namespace LSVJOverlay;

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        using var mutex = new System.Threading.Mutex(true, @"Local\LSVJOverlay-SingleInstance", out bool isFirst);
        if (!isFirst)
        {
            MessageBox.Show("Game Overlay is already running. Look for its icon in the system tray.",
                "Game Overlay", MessageBoxButtons.OK, MessageBoxIcon.Information);
            return;
        }
        ApplicationConfiguration.Initialize();
        Application.Run(new OverlayApplicationContext());
    }
}
