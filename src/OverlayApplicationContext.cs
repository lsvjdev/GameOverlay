// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 lsvjdev

using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace LSVJOverlay;

internal sealed class OverlayApplicationContext : ApplicationContext
{
    private readonly AppSettings _settings = AppSettings.Load();
    private readonly ControlPanel _panel;
    private readonly DesktopStrip _desktop;
    private readonly NotifyIcon _tray;
    private readonly ToolStripMenuItem _gameMenu;
    private readonly ToolStripMenuItem _desktopMenu;
    private readonly System.Windows.Forms.Timer _tick;
    private readonly HardwareReader _hardware = new();
    private readonly RtssClient _rtss = new();
    private readonly GameSessionClock _clock = new();
    private bool _enabled = true;
    private bool _exiting;
    private bool _busy;
    private MetricSnapshot _last = MetricSnapshot.Empty;
    private readonly bool _hotkeyRegistered;

    public OverlayApplicationContext()
    {
        _panel = new ControlPanel(_settings);
        MainForm = _panel;
        _desktop = new DesktopStrip(_settings);

        var menu = new ContextMenuStrip
        {
            BackColor = Color.FromArgb(242, 242, 242),
            ForeColor = Color.FromArgb(30, 30, 30),
            Renderer = new TrayMenuRenderer()
        };
        menu.Items.Add("Open settings", null, (_, _) => OpenSettings());
        _gameMenu = new ToolStripMenuItem("In-game overlay", null, (_, _) => ToggleOverlay());
        _desktopMenu = new ToolStripMenuItem("Desktop preview", null, (_, _) => ToggleDesktop());
        menu.Items.Add(_gameMenu);
        menu.Items.Add(_desktopMenu);
        var sensors = new ToolStripMenuItem("Sensor info");
        sensors.DropDownItems.Add("Copy CPU sensor info", null, (_, _) => CopyCpuSensors());
        sensors.DropDownItems.Add("Copy GPU sensor info", null, (_, _) => CopyGpuSensors());
        menu.Items.Add(sensors);
        menu.Opening += (_, _) => UpdateTrayMenu();
        menu.Items.Add("Reset session timer", null, (_, _) => { _clock.Reset(); });
        menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add("Exit", null, (_, _) => ExitApplication());
        _tray = new NotifyIcon
        {
            Icon = SystemIcons.Application,
            Text = "Game Overlay",
            ContextMenuStrip = menu,
            Visible = true
        };
        _tray.DoubleClick += (_, _) => OpenSettings();
        UpdateTrayMenu();

        _panel.OverlayToggleRequested += ToggleOverlay;
        _panel.DesktopToggleRequested += ToggleDesktop;
        _panel.ResetRequested += () => _clock.Reset();
        _panel.SettingsChanged += () => { _desktop.RefreshLayout(); if (_enabled) _rtss.Publish(RtssClient.BuildRichText(_last, _settings.Use24HourClock)); };
        _panel.FormClosing += (_, e) =>
        {
            if (_exiting) return;
            e.Cancel = true;
            _panel.Hide();
        };
        _panel.Shown += (_, _) => UpdatePanel();

        // Register the hotkey on the settings window, which remains alive when hidden.
        _hotkeyRegistered = Native.RegisterHotKey(_panel.Handle, 10,
            Native.ModControl | Native.ModShift | Native.ModNoRepeat, Native.VkO);
        if (!_hotkeyRegistered) _panel.SetHotkeyWarning();
        Application.AddMessageFilter(new HotkeyFilter(() => ToggleOverlay()));

        _tick = new System.Windows.Forms.Timer { Interval = 1000 };
        _tick.Tick += OnTick;
        _tick.Start();
        _panel.Show();
    }

    private async void OnTick(object? sender, EventArgs e)
    {
        if (_busy || _exiting) return;
        _busy = true;
        try
        {
            // Hardware polling runs off the UI thread; no work occurs per game frame.
            HardwareSample hardware = await Task.Run(() => _hardware.Read());
            if (_exiting) return;
            GameSample? game = _rtss.GetForegroundGame();
            TimeSpan? playTime = _clock.Track(game?.ProcessId ?? 0);
            _last = new MetricSnapshot(
                DateTime.Now, game?.FramesPerSecond, playTime,
                hardware.CpuTempC, hardware.GpuLoadPercent, hardware.GpuTempC, hardware.RamPercent,
                game?.ProcessName ?? "No RTSS-tracked game in foreground");

            if (_enabled)
            {
                _rtss.Publish(RtssClient.BuildRichText(_last, _settings.Use24HourClock));
            }
            _desktop.UpdateMetrics(_last);
            UpdatePanel();
        }
        catch (Exception ex)
        {
            _panel.UpdateState(_last, "Monitoring error: " + ex.Message, _enabled, _desktop.Visible);
        }
        finally { _busy = false; }
    }

    private void UpdatePanel()
    {
        if (!_panel.IsDisposed)
            _panel.UpdateState(_last, _rtss.ConnectionMessage, _enabled, _desktop.Visible);
    }

    private void ToggleOverlay()
    {
        _enabled = !_enabled;
        if (!_enabled) _rtss.Clear();
        else _rtss.Publish(RtssClient.BuildRichText(_last, _settings.Use24HourClock));
        UpdateTrayMenu();
        UpdatePanel();
    }

    private void ToggleDesktop()
    {
        if (_desktop.Visible) _desktop.Hide();
        else _desktop.Show();
        UpdateTrayMenu();
        UpdatePanel();
    }

    private void UpdateTrayMenu()
    {
        _gameMenu.Checked = _enabled;
        _gameMenu.Text = "In-game overlay";
        _desktopMenu.Checked = _desktop.Visible;
        _desktopMenu.Text = "Desktop preview";
    }

    private void CopyCpuSensors()
    {
        try
        {
            Clipboard.SetText(_hardware.LastCpuSensorInfo);
        }
        catch (System.Runtime.InteropServices.ExternalException)
        {
            MessageBox.Show("The clipboard is busy. Try copying again.",
                "Game Overlay", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
    }

    private void CopyGpuSensors()
    {
        try
        {
            Clipboard.SetText(_hardware.LastGpuSensorInfo);
        }
        catch (System.Runtime.InteropServices.ExternalException)
        {
            MessageBox.Show("The clipboard is busy. Try copying again.",
                "Game Overlay", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
    }

    private void OpenSettings()
    {
        if (_panel.Visible) { _panel.Activate(); return; }
        _panel.Show();
        _panel.WindowState = FormWindowState.Normal;
        _panel.Activate();
    }

    private void ExitApplication()
    {
        if (_exiting) return;
        _exiting = true;
        _tick.Stop();
        _rtss.Clear();
        if (_hotkeyRegistered) Native.UnregisterHotKey(_panel.Handle, 10);
        _tray.Visible = false;
        _tray.Dispose();
        _desktop.Close();
        _panel.Close();
        _hardware.Dispose();
        ExitThread();
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing && !_exiting) ExitApplication();
        base.Dispose(disposing);
    }

    private sealed class HotkeyFilter : IMessageFilter
    {
        private readonly Action _callback;
        public HotkeyFilter(Action callback) => _callback = callback;
        public bool PreFilterMessage(ref Message m)
        {
            if (m.Msg != Native.HotkeyMessage || m.WParam.ToInt32() != 10) return false;
            _callback();
            return true;
        }
    }
}

internal sealed class TrayMenuRenderer : ToolStripProfessionalRenderer
{
    private static readonly Color MenuColor = Color.FromArgb(242, 242, 242);
    private static readonly Color CheckColor = Color.FromArgb(150, 201, 239);
    private static readonly Color TickColor = Color.FromArgb(43, 49, 53);

    protected override void OnRenderToolStripBackground(ToolStripRenderEventArgs e)
    {
        using var brush = new SolidBrush(MenuColor);
        e.Graphics.FillRectangle(brush, e.AffectedBounds);
    }

    protected override void OnRenderImageMargin(ToolStripRenderEventArgs e)
    {
        using var brush = new SolidBrush(MenuColor);
        e.Graphics.FillRectangle(brush, e.AffectedBounds);
    }

    protected override void OnRenderMenuItemBackground(ToolStripItemRenderEventArgs e)
    {
        if (!e.Item.Selected) return;
        using var brush = new SolidBrush(Color.FromArgb(224, 232, 239));
        e.Graphics.FillRectangle(brush, new Rectangle(Point.Empty, e.Item.Size));
    }

    protected override void OnRenderToolStripBorder(ToolStripRenderEventArgs e)
    {
        using var pen = new Pen(Color.FromArgb(198, 198, 198));
        var bounds = e.ToolStrip.ClientRectangle;
        if (bounds.Width > 0 && bounds.Height > 0)
            e.Graphics.DrawRectangle(pen, bounds.Left, bounds.Top, bounds.Width - 1, bounds.Height - 1);
    }

    protected override void OnRenderItemCheck(ToolStripItemImageRenderEventArgs e)
    {
        // Soft blue square and a dark tick, without the default Windows blue border.
        int size = Math.Max(16, Math.Min(22, e.Item.Height - 2));
        int left = Math.Max(2, e.ImageRectangle.Left + (e.ImageRectangle.Width - size) / 2);
        int top = (e.Item.Height - size) / 2;

        using (var brush = new SolidBrush(CheckColor))
            e.Graphics.FillRectangle(brush, left, top, size, size);

        var previousSmoothing = e.Graphics.SmoothingMode;
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        using (var pen = new Pen(TickColor, Math.Max(1.5f, size * 0.09f))
            { StartCap = LineCap.Round, EndCap = LineCap.Round, LineJoin = LineJoin.Round })
        {
            e.Graphics.DrawLines(pen, new[]
            {
                new PointF(left + size * 0.27f, top + size * 0.51f),
                new PointF(left + size * 0.44f, top + size * 0.68f),
                new PointF(left + size * 0.77f, top + size * 0.32f)
            });
        }
        e.Graphics.SmoothingMode = previousSmoothing;
    }
}
