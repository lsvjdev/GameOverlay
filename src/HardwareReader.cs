// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 LSVJ-Coding

using System;
using System.Collections.Generic;
using System.Linq;
using LibreHardwareMonitor.Hardware;

namespace LSVJOverlay;

internal sealed class HardwareReader : IDisposable
{
    private Computer? _computer;
    private bool _attemptedInit;

    public string LastGpuSensorInfo { get; private set; } = "No GPU sensor readings available yet.";

    public HardwareSample Read()
    {
        int? ram = null;
        var memory = Native.MemoryStatus.New();
        if (Native.GlobalMemoryStatusEx(ref memory)) ram = (int)memory.MemoryLoad;

        if (!_attemptedInit)
        {
            _attemptedInit = true;
            try
            {
                _computer = new Computer { IsCpuEnabled = true, IsGpuEnabled = true };
                _computer.Open();
            }
            catch
            {
                try { _computer?.Close(); } catch { }
                _computer = null;
            }
        }

        if (_computer is null) return new(null, null, null, ram);
        try
        {
            var hardware = new List<IHardware>();
            foreach (var item in _computer.Hardware)
                UpdateRecursively(item, hardware);

            var cpu = hardware.Where(x => x.HardwareType == HardwareType.Cpu)
                .SelectMany(x => x.Sensors)
                .Where(s => s.SensorType == SensorType.Temperature && s.Value.HasValue)
                .OrderByDescending(s => Preference(s.Name, "CPU Package", "CPU (Tctl/Tdie)", "Core Average", "Core Max"))
                .Select(s => s.Value)
                .FirstOrDefault();
            if (cpu <= 0 || cpu > 125) cpu = null;

            // Prefer a discrete card if present. Keep load and temperature on the same GPU.
            var gpus = hardware.Where(h => h.HardwareType is HardwareType.GpuNvidia or HardwareType.GpuAmd or HardwareType.GpuIntel)
                .OrderBy(h => h.HardwareType == HardwareType.GpuIntel ? 1 : 0)
                .ToArray();
            LastGpuSensorInfo = gpus.Length == 0
                ? "No supported GPU sensors were found."
                : string.Join(Environment.NewLine + Environment.NewLine, gpus.Select(h =>
                    h.Name + " (" + h.HardwareType + ")" + Environment.NewLine +
                    string.Join(Environment.NewLine, h.Sensors
                        .Where(s => s.SensorType is SensorType.Load or SensorType.Temperature)
                        .Select(s => "  " + s.Name + ": " +
                            (s.Value.HasValue ? s.Value.Value.ToString("0.##") : "unavailable") +
                            (s.SensorType == SensorType.Temperature ? " C" : " %")))));
            var gpu = gpus.FirstOrDefault();
            float? load = null;
            float? temperature = null;
            if (gpu is not null)
            {
                load = gpu.Sensors.Where(s => s.SensorType == SensorType.Load && s.Value.HasValue &&
                        (s.Name.Contains("GPU Core", StringComparison.OrdinalIgnoreCase) ||
                         s.Name.Contains("GPU Total", StringComparison.OrdinalIgnoreCase) ||
                         s.Name.Equals("D3D 3D", StringComparison.OrdinalIgnoreCase)))
                    .OrderByDescending(s => Preference(s.Name, "GPU Core", "GPU Total", "GPU Core Load"))
                    .Select(s => s.Value).FirstOrDefault();
                temperature = gpu.Sensors.Where(s => s.SensorType == SensorType.Temperature && s.Value.HasValue &&
                        (s.Name.Contains("GPU Core", StringComparison.OrdinalIgnoreCase) ||
                         s.Name.Contains("GPU Temperature", StringComparison.OrdinalIgnoreCase) ||
                         s.Name.Contains("GPU Edge", StringComparison.OrdinalIgnoreCase) ||
                         s.Name.Contains("GPU Hot Spot", StringComparison.OrdinalIgnoreCase)))
                    .OrderByDescending(s => Preference(s.Name, "GPU Core", "GPU Temperature", "GPU Edge", "GPU Hot Spot"))
                    .Select(s => s.Value).FirstOrDefault();
            }
            if (temperature <= 0 || temperature > 125) temperature = null;
            if (load is < 0 or > 100) load = null;
            return new(cpu, load, temperature, ram);
        }
        catch
        {
            // Sensors are optional; inaccessible sensors must not crash the overlay.
            return new(null, null, null, ram);
        }
    }

    private static int Preference(string name, params string[] preferred)
    {
        for (int i = 0; i < preferred.Length; i++)
            if (name.Equals(preferred[i], StringComparison.OrdinalIgnoreCase))
                return 100 - i;
        return 0;
    }

    private static void UpdateRecursively(IHardware item, List<IHardware> hardware)
    {
        item.Update();
        hardware.Add(item);
        foreach (var child in item.SubHardware) UpdateRecursively(child, hardware);
    }

    public void Dispose()
    {
        try { _computer?.Close(); } catch { }
    }
}
