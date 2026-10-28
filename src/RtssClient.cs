// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 LSVJ-Coding

using System;
using System.IO.MemoryMappedFiles;
using System.Text;

namespace LSVJOverlay;

/// <summary>
/// RTSS SDK v2 shared-memory interface. No DLL injection or game memory edits here:
/// RTSS handles game compatibility and renders our ordinary external OSD client.
/// Layout fields come from RTSSSharedMemory.h; entry sizes/offsets are read dynamically.
/// </summary>
internal sealed class RtssClient
{
    private const string MappingName = "RTSSSharedMemoryV2";
    private const string Owner = "LSVJOverlay-v1";
    // Multi-character C constant 'RTSS', as represented by the SDK.
    private const uint RtssSignature = 0x52545353;
    private const uint MinimumVersion = 0x0002000B; // Rich text formatting tags
    private const int OwnerLength = 256;
    private readonly byte[] _owner = Encoding.ASCII.GetBytes(Owner);

    public string ConnectionMessage { get; private set; } = "RTSS is not running";

    private static bool TryOpen(out MemoryMappedFile? map, out MemoryMappedViewAccessor? view)
    {
        map = null;
        view = null;
        try
        {
            map = MemoryMappedFile.OpenExisting(MappingName, MemoryMappedFileRights.ReadWrite);
            view = map.CreateViewAccessor(0, 0, MemoryMappedFileAccess.ReadWrite);
            if (view.Capacity < 36 || view.ReadUInt32(0) != RtssSignature ||
                view.ReadUInt32(4) < MinimumVersion)
            {
                view.Dispose();
                map.Dispose();
                view = null;
                map = null;
                return false;
            }
            return true;
        }
        catch
        {
            view?.Dispose();
            map?.Dispose();
            map = null;
            view = null;
            return false;
        }
    }

    private static bool ValidRegion(MemoryMappedViewAccessor view, long at, long bytes)
        => at >= 0 && bytes >= 0 && at <= view.Capacity && bytes <= view.Capacity - at;

    private static string ReadCString(MemoryMappedViewAccessor view, long offset, int maxBytes)
    {
        byte[] bytes = new byte[maxBytes];
        view.ReadArray(offset, bytes, 0, bytes.Length);
        int len = Array.IndexOf(bytes, (byte)0);
        return Encoding.ASCII.GetString(bytes, 0, len < 0 ? maxBytes : len);
    }

    private static void WriteCString(MemoryMappedViewAccessor view, long offset, int count, string text)
    {
        byte[] bytes = new byte[count];
        byte[] source = Encoding.ASCII.GetBytes(text);
        Array.Copy(source, bytes, Math.Min(count - 1, source.Length));
        view.WriteArray(offset, bytes, 0, bytes.Length);
    }

    private static void BumpFrame(MemoryMappedViewAccessor view)
    {
        uint frame = view.ReadUInt32(32);
        view.Write(32, unchecked(frame + 1));
    }

    private static bool TryGetOsdLayout(MemoryMappedViewAccessor view, out long start, out int count, out int stride)
    {
        start = view.ReadUInt32(24);
        count = (int)view.ReadUInt32(28);
        stride = (int)view.ReadUInt32(20);
        return count is > 1 and <= 128 && stride >= 512 + 4096 &&
               ValidRegion(view, start, (long)count * stride);
    }

    public GameSample? GetForegroundGame()
    {
        if (!TryOpen(out var map, out var view) || map is null || view is null)
        {
            ConnectionMessage = "RTSS isn't running or its SDK is too old";
            return null;
        }

        using (map)
        using (view)
        {
            ConnectionMessage = "Connected to RTSS";
            IntPtr window = Native.GetForegroundWindow();
            Native.GetWindowThreadProcessId(window, out uint foregroundPid);
            if (foregroundPid == 0) return null;

            long start = view.ReadUInt32(12);
            int count = (int)view.ReadUInt32(16);
            int stride = (int)view.ReadUInt32(8);
            if (count < 1 || count > 4096 || stride < 280 ||
                !ValidRegion(view, start, (long)count * stride)) return null;

            for (int i = 0; i < count; i++)
            {
                long entry = start + (long)i * stride;
                uint pid = view.ReadUInt32(entry);
                if (pid != foregroundPid) continue;
                uint time0 = view.ReadUInt32(entry + 268);
                uint time1 = view.ReadUInt32(entry + 272);
                uint frames = view.ReadUInt32(entry + 276);
                uint durationMs = unchecked(time1 - time0);
                double? fps = time0 != 0 && durationMs is > 0 and < 30000 && frames > 0
                    ? 1000.0 * frames / durationMs : null;
                // Don't show nonsensical values when RTSS has no usable timing data.
                if (fps is <= 0 or > 10000) fps = null;
                string name = ReadCString(view, entry + 4, 260);
                return new GameSample((int)pid, name, fps);
            }
            return null;
        }
    }

    public bool Publish(string richText)
    {
        if (!TryOpen(out var map, out var view) || map is null || view is null)
        {
            ConnectionMessage = "RTSS isn't running or its SDK is too old";
            return false;
        }
        using (map)
        using (view)
        {
            if (!TryGetOsdLayout(view, out long start, out int count, out int stride))
            {
                ConnectionMessage = "RTSS OSD layout isn't compatible";
                return false;
            }

            int chosen = -1;
            // Reuse our own OSD entry if RTSS kept it after a restart.
            for (int i = 1; i < count; i++)
            {
                long at = start + (long)i * stride;
                if (ReadCString(view, at + 256, OwnerLength) == Owner) { chosen = i; break; }
            }
            if (chosen < 0)
            {
                for (int i = 1; i < count; i++)
                {
                    long at = start + (long)i * stride;
                    if (ReadCString(view, at + 256, OwnerLength).Length == 0) { chosen = i; break; }
                }
            }
            if (chosen < 0)
            {
                ConnectionMessage = "RTSS has no free OSD slots";
                return false;
            }

            long slot = start + (long)chosen * stride;
            WriteCString(view, slot + 256, OwnerLength, Owner);
            // SDK: szOSDEx (v2.7+) starts immediately after 256 + 256 bytes.
            WriteCString(view, slot + 512, 4096, richText);
            // Keep the legacy slot empty so it cannot duplicate the formatted text.
            WriteCString(view, slot, 256, "");
            BumpFrame(view);
            ConnectionMessage = "Connected to RTSS";
            return true;
        }
    }

    public void Clear()
    {
        if (!TryOpen(out var map, out var view) || map is null || view is null) return;
        using (map)
        using (view)
        {
            if (!TryGetOsdLayout(view, out long start, out int count, out int stride)) return;
            bool cleared = false;
            for (int i = 1; i < count; i++)
            {
                long at = start + (long)i * stride;
                if (ReadCString(view, at + 256, OwnerLength) != Owner) continue;
                WriteCString(view, at, 256, "");
                WriteCString(view, at + 512, 4096, "");
                WriteCString(view, at + 256, 256, "");
                cleared = true;
            }
            if (cleared) BumpFrame(view);
        }
    }

    // Stable RTSS OSD format tags: define grey/green palettes, then apply to each span.
    public static string BuildRichText(MetricSnapshot m, bool use24HourClock = true)
    {
        static string Grey(string text) => $"<C0>{text}<C>";
        static string Green(string text) => $"<C1>{text}<C>";
        return "<C0=BDB5BC><C1=52D955>\r" +
               Grey("FPS ") + Green(m.FpsText) + "        " +
               Grey("Time ") + Green(m.GetTimeText(use24HourClock)) + "        " +
               Grey("Playtime ") + Green(m.PlaytimeText) + "        " +
               Grey("CPU ") + Green(m.CpuText) + "        " +
               Grey("GPU ") + Green(m.GpuLoadText + "  " + m.GpuTempText) + "        " +
               Grey("RAM ") + Green(m.RamText) + "     " +
               Grey("(Ctrl + Shift + O)");
    }
}
