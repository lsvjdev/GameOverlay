// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 LSVJ-Coding

using System;
using System.Drawing;
using System.Drawing.Text;
using System.Windows.Forms;

namespace LSVJOverlay;

internal static class StripPainter
{
    public const int BaseWidth = 780;
    public const int BaseHeight = 28;

    private static readonly Color BackgroundColor = Color.FromArgb(18, 19, 21);
    private static readonly Color LabelColor = Color.FromArgb(196, 190, 199);
    private static readonly Color ValueColor = Color.FromArgb(81, 216, 91);

    private static (string Text, bool Value, bool GroupEnd)[] GetParts(MetricSnapshot data, bool use24HourClock)
        => new (string Text, bool Value, bool GroupEnd)[]
        {
            ("FPS ", false, false), (data.FpsText, true, true),
            ("Time ", false, false), (data.GetTimeText(use24HourClock), true, true),
            ("Playtime ", false, false), (data.PlaytimeText, true, true),
            ("CPU ", false, false), (data.CpuText, true, true),
            ("GPU ", false, false), (data.GpuLoadText, true, false),
            ("  ", false, false), (data.GpuTempText, true, true),
            ("RAM ", false, false), (data.RamText, true, true),
            ("(Ctrl + Shift + O)", false, false)
        };

    // Match the window width to the actual text, not a fixed 780px strip.
    // This also accounts for different DPI scales, clock formats and available values.
    public static int MeasureWidth(Graphics g, MetricSnapshot data, bool use24HourClock)
    {
        using var font = new Font("Segoe UI", 9.2f, FontStyle.Bold, GraphicsUnit.Point);
        using var format = (StringFormat)StringFormat.GenericTypographic.Clone();
        format.FormatFlags |= StringFormatFlags.MeasureTrailingSpaces | StringFormatFlags.NoWrap;
        float total = 0;
        foreach (var (text, _, groupEnd) in GetParts(data, use24HourClock))
        {
            total += g.MeasureString(text, font, PointF.Empty, format).Width;
            if (groupEnd) total += 18f;
        }
        return (int)Math.Ceiling(total + 16f);
    }

    public static void Paint(Graphics g, Rectangle canvas, MetricSnapshot data, bool use24HourClock = true)
    {
        using (var background = new SolidBrush(BackgroundColor))
            g.FillRectangle(background, canvas);

        g.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;
        using var font = new Font("Segoe UI", 9.2f, FontStyle.Bold, GraphicsUnit.Point);
        using var labels = new SolidBrush(LabelColor);
        using var numbers = new SolidBrush(ValueColor);
        using var format = (StringFormat)StringFormat.GenericTypographic.Clone();
        format.FormatFlags |= StringFormatFlags.MeasureTrailingSpaces | StringFormatFlags.NoWrap;

        var parts = GetParts(data, use24HourClock);

        const float gap = 18f;
        float contentWidth = 0;
        foreach (var (text, _, groupEnd) in parts)
        {
            contentWidth += g.MeasureString(text, font, PointF.Empty, format).Width;
            if (groupEnd) contentWidth += gap;
        }

        float horizontalScale = Math.Min(1f, (canvas.Width - 16f) / Math.Max(1f, contentWidth));
        var state = g.Save();
        g.TranslateTransform(canvas.Left + 8f, 0);
        g.ScaleTransform(horizontalScale, 1f);

        float x = 0;
        float y = canvas.Top + (canvas.Height - font.GetHeight(g)) / 2f;
        foreach (var (text, value, groupEnd) in parts)
        {
            g.DrawString(text, font, value ? numbers : labels, new PointF(x, y), format);
            x += g.MeasureString(text, font, PointF.Empty, format).Width;
            if (groupEnd) x += gap;
        }
        g.Restore(state);
    }
}

internal sealed class DesktopStrip : Form
{
    private MetricSnapshot _snapshot = MetricSnapshot.Empty;
    private readonly AppSettings _settings;

    public DesktopStrip(AppSettings settings)
    {
        _settings = settings;
        FormBorderStyle = FormBorderStyle.None;
        ShowInTaskbar = false;
        TopMost = true;
        StartPosition = FormStartPosition.Manual;
        Width = Math.Min(StripPainter.BaseWidth, (Screen.PrimaryScreen?.WorkingArea.Width ?? 1920) - 24);
        Height = StripPainter.BaseHeight;
        UpdatePosition();
        DoubleBuffered = true;
        Opacity = 0.68;
    }

    public void UpdatePosition()
    {
        var work = Screen.PrimaryScreen?.WorkingArea ?? new Rectangle(0, 0, 1920, 1080);
        int left = _settings.DesktopPosition switch
        {
            StripPosition.TopCenter => work.Left + (work.Width - Width) / 2,
            StripPosition.TopRight => work.Right - Width - 12,
            _ => work.Left + 12
        };
        Location = new Point(Math.Max(work.Left, left), work.Top + 8);
    }

    protected override bool ShowWithoutActivation => true;

    protected override CreateParams CreateParams
    {
        get
        {
            var cp = base.CreateParams;
            cp.ExStyle |= 0x00000080 /* tool window */ | 0x08000000 /* no activate */ |
                          0x00000020 /* click through */;
            return cp;
        }
    }

    public void RefreshLayout()
    {
        using var graphics = CreateGraphics();
        var work = Screen.PrimaryScreen?.WorkingArea ?? new Rectangle(0, 0, 1920, 1080);
        int maxWidth = Math.Max(1, work.Width - 24);
        int width = Math.Clamp(StripPainter.MeasureWidth(graphics, _snapshot, _settings.Use24HourClock), 1, maxWidth);
        if (Width != width) Width = width;
        UpdatePosition();
        if (Visible) Invalidate();
    }

    public void UpdateMetrics(MetricSnapshot snapshot)
    {
        _snapshot = snapshot;
        RefreshLayout();
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        StripPainter.Paint(e.Graphics, ClientRectangle, _snapshot, _settings.Use24HourClock);
    }
}
