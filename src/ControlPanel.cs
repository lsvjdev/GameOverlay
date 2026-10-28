// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 LSVJ-Coding

using System;
using System.Drawing;
using System.Windows.Forms;

namespace LSVJOverlay;

internal sealed class ControlPanel : Form
{
    private readonly AppSettings _settings;
    private readonly Label _connection;
    private readonly Label _game;
    private readonly Button _overlayButton;
    private readonly Button _desktopButton;
    private readonly PreviewBox _preview;

    public event Action? OverlayToggleRequested;
    public event Action? DesktopToggleRequested;
    public event Action? ResetRequested;
    public event Action? SettingsChanged;

    public ControlPanel(AppSettings settings)
    {
        _settings = settings;
        Text = "Game Overlay";
        ClientSize = new Size(640, 285);
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = false;
        BackColor = Color.FromArgb(24, 23, 29);
        ForeColor = Color.Gainsboro;
        StartPosition = FormStartPosition.CenterScreen;
        Font = new Font("Segoe UI", 9);

        Controls.Add(new Label {
            Text = "Game Overlay", Location = new Point(18, 16),
            Size = new Size(290, 28), Font = new Font("Segoe UI", 15, FontStyle.Bold),
            ForeColor = Color.White
        });

        _preview = new PreviewBox(_settings) {
            Location = new Point(18, 52), Size = new Size(604, 28)
        };
        Controls.Add(_preview);

        _overlayButton = MakeButton("Hide in-game overlay", 18, 103, 176);
        _overlayButton.Click += (_, _) => OverlayToggleRequested?.Invoke();
        _desktopButton = MakeButton("Show desktop preview", 204, 103, 176);
        _desktopButton.Click += (_, _) => DesktopToggleRequested?.Invoke();
        var resetButton = MakeButton("Reset timer", 390, 103, 112);
        resetButton.Click += (_, _) => ResetRequested?.Invoke();
        Controls.AddRange(new Control[] { _overlayButton, _desktopButton, resetButton });

        Controls.Add(new Label {
            Text = "Clock", Location = new Point(18, 159), Size = new Size(45, 24),
            ForeColor = Color.Gainsboro
        });
        var clock = new ComboBox {
            Location = new Point(66, 155), Width = 110,
            DropDownStyle = ComboBoxStyle.DropDownList
        };
        clock.Items.AddRange(new object[] { "24-hour", "12-hour" });
        clock.SelectedIndex = _settings.Use24HourClock ? 0 : 1;
        clock.SelectedIndexChanged += (_, _) => {
            _settings.Use24HourClock = clock.SelectedIndex == 0;
            Changed();
        };
        Controls.Add(clock);

        Controls.Add(new Label {
            Text = "Desktop position", Location = new Point(207, 159),
            Size = new Size(108, 24), ForeColor = Color.Gainsboro
        });
        var pos = new ComboBox {
            Location = new Point(318, 155), Width = 137,
            DropDownStyle = ComboBoxStyle.DropDownList
        };
        pos.Items.AddRange(new object[] { "Top left", "Top center", "Top right" });
        pos.SelectedIndex = (int)_settings.DesktopPosition;
        pos.SelectedIndexChanged += (_, _) => {
            _settings.DesktopPosition = (StripPosition)pos.SelectedIndex;
            Changed();
        };
        Controls.Add(pos);

        _connection = new Label {
            Text = "RTSS: Not connected", Location = new Point(18, 204),
            Size = new Size(604, 22), AutoEllipsis = true
        };
        _game = new Label {
            Text = "Game: None detected", Location = new Point(18, 229),
            Size = new Size(604, 22), AutoEllipsis = true,
            ForeColor = Color.Silver
        };
        Controls.AddRange(new Control[] { _connection, _game });
        Controls.Add(new Label {
            Text = "Ctrl + Shift + O to toggle  |  Close to minimize to tray",
            Location = new Point(18, 258), Size = new Size(604, 20),
            ForeColor = Color.Gray, Font = new Font("Segoe UI", 8)
        });
    }

    private void Changed()
    {
        _settings.Save();
        _preview.Invalidate();
        SettingsChanged?.Invoke();
    }

    private static Button MakeButton(string text, int x, int y, int width) => new() {
        Text = text, Location = new Point(x, y), Size = new Size(width, 34),
        FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(47, 42, 55),
        ForeColor = Color.White, TabStop = false
    };

    public void UpdateState(MetricSnapshot snapshot, string status, bool enabled, bool desktopShown)
    {
        _preview.Snapshot = snapshot;
        _connection.Text = "RTSS: " + status;
        _connection.ForeColor = status.StartsWith("Connected", StringComparison.Ordinal)
            ? Color.FromArgb(90, 212, 104) : Color.FromArgb(230, 168, 92);
        _game.Text = "Game: " + snapshot.GameName;
        _overlayButton.Text = enabled ? "Hide in-game overlay" : "Show in-game overlay";
        _desktopButton.Text = desktopShown ? "Hide desktop preview" : "Show desktop preview";
    }

    public void SetHotkeyWarning() { }

    private sealed class PreviewBox : Panel
    {
        private readonly AppSettings _settings;
        private MetricSnapshot _snapshot = MetricSnapshot.Empty;
        public MetricSnapshot Snapshot { get => _snapshot; set { _snapshot = value; Invalidate(); } }
        public PreviewBox(AppSettings settings) { _settings = settings; DoubleBuffered = true; }
        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            int targetWidth = StripPainter.MeasureWidth(e.Graphics, _snapshot, _settings.Use24HourClock);
            using var bmp = new Bitmap(Math.Max(1, targetWidth), StripPainter.BaseHeight);
            using (var g = Graphics.FromImage(bmp))
                StripPainter.Paint(g, new Rectangle(0, 0, bmp.Width, bmp.Height), _snapshot, _settings.Use24HourClock);
            e.Graphics.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
            float scale = Math.Min(1f, ClientSize.Width / (float)bmp.Width);
            int drawWidth = (int)Math.Ceiling(bmp.Width * scale);
            e.Graphics.DrawImage(bmp, new Rectangle(0, 0, drawWidth, ClientSize.Height));
        }
    }
}
