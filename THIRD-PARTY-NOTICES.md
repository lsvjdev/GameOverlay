# Third-party notices — LSVJ Overlay

LSVJ Overlay original application code is licensed under the **GNU General Public License, version 3 only (GPL-3.0-only)**. See [LICENSE](LICENSE).

Third-party components retain their own copyright and licensing. They are **not** owned or relicensed by LSVJ-Coding.

## Components and dependencies

| Component | Use | License / notices |
| --- | --- | --- |
| [LibreHardwareMonitorLib 0.9.6](https://www.nuget.org/packages/LibreHardwareMonitorLib/0.9.6) | Direct sensor library | Mozilla Public License 2.0; upstream third-party notices |
| [DiskInfoToolkit >=1.1.2](https://www.nuget.org/packages/DiskInfoToolkit/1.1.2) | Transitive dependency | MPL-2.0 |
| [RAMSPDToolkit-NDD >=1.4.2](https://www.nuget.org/packages/RAMSPDToolkit-NDD/1.4.2) | Transitive dependency | MPL-2.0 |
| [BlackSharp.Core](https://www.nuget.org/packages/BlackSharp.Core) | Transitive dependency of RAMSPDToolkit | MPL-2.0 |
| [HidSharp >=2.6.4](https://www.nuget.org/packages/HidSharp/2.6.4) | Transitive dependency | Apache-2.0 |
| [Mono.Posix.NETStandard 1.0.0](https://www.nuget.org/packages/Mono.Posix.NETStandard/1.0.0) | Transitive dependency | MIT / Mono Project |
| Microsoft .NET and Microsoft System.* packages | Runtime and platform libraries | Predominantly MIT, with additional Windows-specific licensing |

The full text of the known licenses is stored in the LICENSES directory.
The [upstream LibreHardwareMonitor 0.9.6 notices](LICENSES/LibreHardwareMonitor-THIRD-PARTY-NOTICES.txt)
are reproduced here unchanged, including upstream mentions of **Aga.Controls**
(BSD-like) and **PawnIO.Modules** (LGPL-2.1). Inclusion in upstream notices
does NOT mean both components are included in the LSVJ published binary.

Direct and transitive NuGet dependencies can change with restore. Verify the
actual resolved dependency graph and exact binary payload before every release.
A way to list resolved dependencies is:
    
    dotnet list src/LSVJOverlay.csproj package --include-transitive

## Self-contained Windows .NET licensing

The AppVeyor build publishes a self-contained Windows .NET application. Parts
of the Windows runtime (such as coreclr.dll and the .NET runtime inside
single-file executables) are subject to Microsoft's **.NET Library License**,
not necessarily the MIT source-code license. Include any applicable runtime
license and exact version's third-party notices in final release distributions.

- [.NET licensing on Windows](https://github.com/dotnet/core/blob/main/license-information-windows.md)
- [Microsoft .NET Library License](https://dotnet.microsoft.com/en-us/dotnet_library_license.htm)
- [.NET runtime third-party notices](https://github.com/dotnet/runtime/blob/main/THIRD-PARTY-NOTICES.TXT)

The supplied MIT-DotNet.txt is the Microsoft .NET **source** MIT license;
it does not override additional binary licenses. Review the terms of
redistributed .NET binaries before shipping a release.

## RTSS is external

RivaTuner Statistics Server (RTSS) is installed **separately** by users.
LSVJ Overlay communicates through its external shared-memory interface.
The repository and build **do not include RTSS binaries, drivers or installer**.
No permission to redistribute RTSS is implied by our GPL license.

## Release and attribution requirements

- Keep the GNU GPL-3.0-only license and copyright notice for the LSVJ code.
- Preserve all third-party notices and their separate license requirements.
- Provide access to the **corresponding source of each distributed build**,
  including build scripts. Keep an immutable source tag for official releases.
- Re-check any LGPL objects actually bundled in the release, especially when
  creating single-file bundles. The distributed build must meet all relevant
  obligations or be changed accordingly.
- Do not reuse Razer, RTSS, MSI or other vendors' trademarks, logos or assets
  without appropriate permission. This project is independent and not endorsed
  by those vendors.

This notice is based on the declared dependencies and published metadata, not
an audit of a finished compiled release. It is not legal advice.
