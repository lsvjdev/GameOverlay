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
            MessageBox.Show("LSVJ Overlay is already running. Look for its icon in the system tray.",
                "LSVJ Overlay", MessageBoxButtons.OK, MessageBoxIcon.Information);
            return;
        }
        ApplicationConfiguration.Initialize();
        Application.Run(new OverlayApplicationContext());
    }
}
