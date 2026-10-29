// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 lsvjdev

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
    public string LastCpuSensorInfo { get; private set; } = "No CPU sensor readings available yet.";

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
            catch (Exception ex)
            {
                LastCpuSensorInfo = "Hardware monitoring could not start: " + ex.Message;
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

            var cpus = hardware.Where(h => h.HardwareType == HardwareType.Cpu).ToArray();
            LastCpuSensorInfo = cpus.Length == 0
                ? "LibreHardwareMonitor did not find a CPU hardware entry."
                : string.Join(Environment.NewLine + Environment.NewLine, cpus.Select(h =>
                    h.Name + " (" + h.HardwareType + ")" + Environment.NewLine +
                    string.Join(Environment.NewLine, h.Sensors
                        .Where(s => s.SensorType is SensorType.Temperature or SensorType.Load or SensorType.Clock)
                        .Select(s => "  [" + s.SensorType + "] " + s.Name + ": " +
                            (s.Value.HasValue ? s.Value.Value.ToString("0.##") : "unavailable") +
                            (s.SensorType == SensorType.Temperature ? " C" :
                             s.SensorType == SensorType.Load ? " %" : " MHz")))));

            // Ignore invalid values before ranking the temperature sensors.
            // A zero-valued preferred sensor must not hide a valid alternative.
            float? cpu = cpus.SelectMany(h => h.Sensors)
                .Where(s => s.SensorType == SensorType.Temperature && s.Value is > 0 and <= 125)
                .OrderByDescending(s => Preference(s.Name,
                    "CPU Package", "CPU (Tctl/Tdie)", "CPU Tctl/Tdie", "Core Average", "Core Max"))
                .Select(s => s.Value)
                .FirstOrDefault();

            // Prefer NVIDIA when available, then AMD, then Intel.
            // This avoids picking the integrated Vega graphics before the GTX 1650.
            // Keep GPU load and temperature readings on the same device.
            var gpus = hardware.Where(h => h.HardwareType is HardwareType.GpuNvidia or HardwareType.GpuAmd or HardwareType.GpuIntel)
                .OrderBy(h => h.HardwareType switch
                {
                    HardwareType.GpuNvidia => 0,
                    HardwareType.GpuAmd => 1,
                    _ => 2
                })
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
        catch (Exception ex)
        {
            // Sensor failures should not crash the overlay. Keep the error available for diagnostics.
            LastCpuSensorInfo = "CPU sensor polling failed: " + ex.Message;
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
