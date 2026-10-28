// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 LSVJ-Coding

using System;
using System.Drawing;
using System.Windows.Forms;

namespace LSVJOverlay;

internal sealed class ControlPanel : Form
{
    private readonly Label _connection;
    private readonly Label _game;
    private readonly Label _hotkey;
    private readonly Button _overlayButton;
    private readonly Button _desktopButton;
    private readonly PreviewBox _preview;

    public event Action? OverlayToggleRequested;
    public event Action? DesktopToggleRequested;
    public event Action? ResetRequested;

    public ControlPanel()
    {
        Text = "LSVJ Overlay • Razer-style performance strip";
        ClientSize = new Size(860, 325);
        MinimumSize = new Size(700, 360);
        FormBorderStyle = FormBorderStyle.FixedSingle;
        MaximizeBox = false;
        BackColor = Color.FromArgb(22, 20, 27);
        ForeColor = Color.Gainsboro;
        StartPosition = FormStartPosition.CenterScreen;
        Font = new Font("Segoe UI", 10);

        var title = Label("LSVJ PERFORMANCE OVERLAY", 24, 19, 690, 35, 19, Color.White, true);
        var sub = Label("Native Windows app · RTSS fullscreen renderer · 1-second hardware polling", 25, 59, 800, 23, 10,
            Color.FromArgb(166, 161, 172));
        Controls.Add(title);
        Controls.Add(sub);

        _preview = new PreviewBox { Location = new Point(24, 99), Size = new Size(812, 42), BackColor = Color.FromArgb(50, 41, 55) };
        Controls.Add(_preview);

        _overlayButton = MakeButton("Hide game overlay", 24, 166, 190, 42);
        _overlayButton.Click += (_, _) => OverlayToggleRequested?.Invoke();
        _desktopButton = MakeButton("Show desktop strip", 224, 166, 190, 42);
        _desktopButton.Click += (_, _) => DesktopToggleRequested?.Invoke();
        var resetButton = MakeButton("Reset playtime", 424, 166, 150, 42);
        resetButton.Click += (_, _) => ResetRequested?.Invoke();
        Controls.Add(_overlayButton);
        Controls.Add(_desktopButton);
        Controls.Add(resetButton);

        _connection = Label("RTSS: connecting...", 24, 236, 820, 23, 10, Color.FromArgb(195, 188, 204));
        _game = Label("Game: waiting for a fullscreen game", 24, 261, 820, 23, 10, Color.FromArgb(195, 188, 204));
        _hotkey = Label("Global hotkey: Ctrl + Shift + O   •   Closing this window minimizes to the tray", 24, 292, 820, 22, 9, Color.FromArgb(133, 129, 141));
        Controls.Add(_connection);
        Controls.Add(_game);
        Controls.Add(_hotkey);
    }

    private static Label Label(string text, int x, int y, int w, int h, int size, Color color, bool bold = false)
        => new()
        {
            Text = text,
            Location = new Point(x, y),
            Size = new Size(w, h),
            AutoEllipsis = true,
            Font = new Font("Segoe UI", size, bold ? FontStyle.Bold : FontStyle.Regular),
            ForeColor = color
        };

    private static Button MakeButton(string label, int x, int y, int w, int h)
        => new()
        {
            Text = label,
            Location = new Point(x, y),
            Size = new Size(w, h),
            FlatStyle = FlatStyle.Flat,
            ForeColor = Color.White,
            BackColor = Color.FromArgb(51, 45, 60),
            Cursor = Cursors.Hand,
            TabStop = false
        };

    public void UpdateState(MetricSnapshot snapshot, string status, bool enabled, bool desktopShown)
    {
        _preview.Snapshot = snapshot;
        _connection.Text = "RTSS: " + status;
        _connection.ForeColor = status.StartsWith("Connected", StringComparison.Ordinal)
            ? Color.FromArgb(93, 218, 103) : Color.FromArgb(239, 179, 102);
        _game.Text = "Game: " + snapshot.GameName;
        _overlayButton.Text = enabled ? "Hide game overlay" : "Show game overlay";
        _desktopButton.Text = desktopShown ? "Hide desktop strip" : "Show desktop strip";
    }

    public void SetHotkeyWarning()
    {
        _hotkey.Text = "Ctrl + Shift + O is taken by another app. Use the tray menu to toggle instead.";
        _hotkey.ForeColor = Color.FromArgb(236, 167, 96);
    }

    private sealed class PreviewBox : Panel
    {
        private MetricSnapshot _snapshot = MetricSnapshot.Empty;
        public MetricSnapshot Snapshot { get => _snapshot; set { _snapshot = value; Invalidate(); } }
        public PreviewBox() { DoubleBuffered = true; }
        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            using var bitmap = new Bitmap(StripPainter.BaseWidth, StripPainter.BaseHeight);
            using (var g = Graphics.FromImage(bitmap))
                StripPainter.Paint(g, new Rectangle(0, 0, bitmap.Width, bitmap.Height), _snapshot);
            e.Graphics.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
            e.Graphics.DrawImage(bitmap, ClientRectangle);
        }
    }
}
