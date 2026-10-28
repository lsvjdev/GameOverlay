using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;

namespace LSVJOverlay;

/// <summary>Screenshot-based layout coordinates, rescaled from a 1229px-wide reference.</summary>
internal static class StripPainter
{
    public const int BaseWidth = 1229;
    public const int BaseHeight = 40;
    private static readonly Color LabelColor = Color.FromArgb(190, 180, 188);
    private static readonly Color ValueColor = Color.FromArgb(77, 218, 82);

    public static void Paint(Graphics g, Rectangle canvas, MetricSnapshot data)
    {
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.Clear(Color.FromArgb(56, 43, 56));
        float scale = Math.Min(1, canvas.Width / (float)BaseWidth);
        float fontSize = scale < 0.88f ? 11.0f : 12.0f;
        using var font = new Font("Segoe UI", fontSize, FontStyle.Bold, GraphicsUnit.Point);
        const TextFormatFlags flags = TextFormatFlags.NoPadding | TextFormatFlags.SingleLine;

        void T(string text, float x, bool green)
        {
            int px = (int)(x * scale);
            var size = TextRenderer.MeasureText(g, text, font, new Size(int.MaxValue, canvas.Height), flags);
            TextRenderer.DrawText(g, text, font,
                new Point(canvas.Left + px, canvas.Top + (canvas.Height - size.Height) / 2),
                green ? ValueColor : LabelColor, flags);
        }

        T("FPS", 14, false); T(data.FpsText, 62, true);
        T("Time", 143, false); T(data.TimeText, 205, true);
        T("Playtime", 358, false); T(data.PlaytimeText, 453, true);
        T("CPU", 541, false); T(data.CpuText, 597, true);
        T("GPU", 689, false); T(data.GpuLoadText, 745, true); T(data.GpuTempText, 807, true);
        T("RAM", 891, false); T(data.RamText, 948, true);
        T("(Ctrl + Shift + O)", 1024, false);
    }
}

internal sealed class DesktopStrip : Form
{
    private MetricSnapshot _snapshot = MetricSnapshot.Empty;

    public DesktopStrip()
    {
        FormBorderStyle = FormBorderStyle.None;
        ShowInTaskbar = false;
        TopMost = true;
        StartPosition = FormStartPosition.Manual;
        Width = Math.Min(StripPainter.BaseWidth, Screen.PrimaryScreen?.WorkingArea.Width - 20 ?? StripPainter.BaseWidth);
        Height = StripPainter.BaseHeight;
        Location = new Point(Math.Max(0, (Screen.PrimaryScreen?.Bounds.Width ?? Width) / 2 - Width / 2), 22);
        DoubleBuffered = true;
        Opacity = .93;
    }

    protected override bool ShowWithoutActivation => true;
    protected override CreateParams CreateParams
    {
        get
        {
            var cp = base.CreateParams;
            cp.ExStyle |= 0x00000080 /* TOOLWINDOW */ | 0x08000000 /* NOACTIVATE */ |
                          0x00000020 /* TRANSPARENT, click-through */;
            return cp;
        }
    }

    public void UpdateMetrics(MetricSnapshot snapshot)
    {
        _snapshot = snapshot;
        if (Visible) Invalidate();
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        StripPainter.Paint(e.Graphics, ClientRectangle, _snapshot);
    }
}
