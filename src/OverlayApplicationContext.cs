// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 LSVJ-Coding

using System;
using System.Drawing;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace LSVJOverlay;

internal sealed class OverlayApplicationContext : ApplicationContext
{
    private readonly ControlPanel _panel;
    private readonly DesktopStrip _desktop;
    private readonly NotifyIcon _tray;
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
        _panel = new ControlPanel();
        MainForm = _panel;
        _desktop = new DesktopStrip();

        var menu = new ContextMenuStrip();
        menu.Items.Add("Open settings", null, (_, _) => OpenSettings());
        menu.Items.Add("Toggle in-game overlay", null, (_, _) => ToggleOverlay());
        menu.Items.Add("Toggle desktop strip", null, (_, _) => ToggleDesktop());
        menu.Items.Add("Reset session timer", null, (_, _) => { _clock.Reset(); });
        menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add("Exit", null, (_, _) => ExitApplication());
        _tray = new NotifyIcon
        {
            Icon = SystemIcons.Application,
            Text = "LSVJ Performance Overlay",
            ContextMenuStrip = menu,
            Visible = true
        };
        _tray.DoubleClick += (_, _) => OpenSettings();

        _panel.OverlayToggleRequested += ToggleOverlay;
        _panel.DesktopToggleRequested += ToggleDesktop;
        _panel.ResetRequested += () => _clock.Reset();
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
        _panel.KeyPreview = true;
        _panel.HandleCreated += (_, _) => { }; // the window stays alive while tray app runs
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
                _rtss.Publish(RtssClient.BuildRichText(_last));
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
        else _rtss.Publish(RtssClient.BuildRichText(_last));
        UpdatePanel();
    }

    private void ToggleDesktop()
    {
        if (_desktop.Visible) _desktop.Hide();
        else _desktop.Show();
        UpdatePanel();
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
