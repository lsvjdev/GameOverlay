namespace LSVJOverlay;

internal readonly record struct HardwareSample(float? CpuTempC, float? GpuLoadPercent, float? GpuTempC, int? RamPercent);
internal readonly record struct GameSample(int ProcessId, string ProcessName, double? FramesPerSecond);

internal sealed record MetricSnapshot(
    DateTime LocalTime, double? Fps, TimeSpan? GameTime,
    float? CpuTempC, float? GpuLoad, float? GpuTempC, int? RamPercent,
    string GameName)
{
    public static MetricSnapshot Empty => new(DateTime.Now, null, null, null, null, null, null, "No game detected");
    public string FpsText => Fps.HasValue ? Math.Round(Fps.Value).ToString("0") : "--";
    public string TimeText => LocalTime.ToString("hh:mm:ss tt", System.Globalization.CultureInfo.InvariantCulture);
    public string PlaytimeText => GameTime is { } t
        ? t.TotalHours >= 1 ? $"{(int)t.TotalHours:00}:{t.Minutes:00}" : $"{t.Minutes:00}:{t.Seconds:00}"
        : "--:--";
    public string CpuText => CpuTempC.HasValue ? $"{CpuTempC.Value:0}°C" : "--";
    public string GpuLoadText => GpuLoad.HasValue ? $"{GpuLoad.Value:0}%" : "--";
    public string GpuTempText => GpuTempC.HasValue ? $"{GpuTempC.Value:0}°C" : "--";
    public string RamText => RamPercent.HasValue ? $"{RamPercent.Value}%" : "--";
}

internal sealed class GameSessionClock
{
    private int _gamePid;
    private DateTime _activeSinceUtc;
    private TimeSpan _accumulated;
    private bool _running;

    public TimeSpan? Track(int detectedPid)
    {
        DateTime now = DateTime.UtcNow;
        if (detectedPid != 0 && detectedPid != _gamePid)
        {
            _gamePid = detectedPid;
            _accumulated = TimeSpan.Zero;
            _activeSinceUtc = now;
            _running = true;
        }
        else if (detectedPid == 0 && _running)
        {
            _accumulated += now - _activeSinceUtc;
            _running = false;
        }
        else if (detectedPid == _gamePid && detectedPid != 0 && !_running)
        {
            _activeSinceUtc = now;
            _running = true;
        }

        if (detectedPid == 0) return null;
        return _accumulated + (_running ? now - _activeSinceUtc : TimeSpan.Zero);
    }

    public void Reset()
    {
        _gamePid = 0;
        _accumulated = TimeSpan.Zero;
        _running = false;
    }
}
