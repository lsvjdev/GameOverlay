# Third-party notices

LSVJ Overlay's original code is licensed under [GPL-3.0-only](LICENSE). Dependencies keep their own licenses and copyrights.

## Dependencies

| Package | License | Use |
| --- | --- | --- |
| [LibreHardwareMonitorLib 0.9.6](https://www.nuget.org/packages/LibreHardwareMonitorLib/0.9.6) | MPL-2.0 | Hardware sensor readings |
| [DiskInfoToolkit](https://www.nuget.org/packages/DiskInfoToolkit) | MPL-2.0 | Indirect dependency |
| [RAMSPDToolkit-NDD](https://www.nuget.org/packages/RAMSPDToolkit-NDD) | MPL-2.0 | Indirect dependency |
| [BlackSharp.Core](https://www.nuget.org/packages/BlackSharp.Core) | MPL-2.0 | Indirect dependency |
| [HidSharp](https://www.nuget.org/packages/HidSharp) | Apache-2.0 | Indirect dependency |
| [Mono.Posix.NETStandard](https://www.nuget.org/packages/Mono.Posix.NETStandard) | MIT | Indirect dependency |
| Microsoft .NET runtime and system packages | Varies by component | Windows app runtime |

These are the known dependencies from the current project's package references and upstream information. The final set of packages depends on the exact restore and publish output.

Relevant license texts are in [LICENSES](LICENSES). The original [LibreHardwareMonitor third-party notices](LICENSES/LibreHardwareMonitor-THIRD-PARTY-NOTICES.txt) are also included. Those notices contain the full LGPL 2.1 text and attribution for PawnIO.Modules, plus the BSD-style notice for Aga.Controls. An upstream notice does not necessarily mean a component ships in our build.

Run `dotnet list src/LSVJOverlay.csproj package --include-transitive` to check resolved packages before publishing a release.

## Windows .NET runtime

The current build creates a self-contained Windows executable. Some Microsoft runtime binaries have terms beyond the MIT license used for the .NET source code. Before distributing a release or installer, check the exact bundled runtime and include its applicable notices.

- [.NET licensing on Windows](https://github.com/dotnet/core/blob/main/license-information-windows.md)
- [.NET Library License](https://dotnet.microsoft.com/en-us/dotnet_library_license.htm)
- [.NET runtime third-party notices](https://github.com/dotnet/runtime/blob/main/THIRD-PARTY-NOTICES.TXT)

## RTSS

RivaTuner Statistics Server is a separate installation. LSVJ Overlay communicates with RTSS, but does not distribute RTSS, its drivers, or its installer.

## Distributing builds

Keep the GPL license, the applicable third-party notices, and the corresponding source for the version being distributed. For official releases, tag the matching source commit.

LSVJ Overlay is not affiliated with or endorsed by Razer, MSI, or RTSS's developers.
