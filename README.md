# LSVJ Overlay

LSVJ Overlay is a small Windows performance overlay that uses RivaTuner Statistics Server (RTSS) to display system stats in games.

The goal is a clean, compact overlay without a separate game booster or a large background app.

## Current status

**Early prototype.** The project has not yet been tested in a Windows game. Some features may not work as expected, and RTSS support depends on the game. There is no official installer or stable release yet.

The overlay is designed to show FPS, time, playtime, CPU and GPU readings, and RAM usage. It also has a desktop preview and a `Ctrl + Shift + O` hotkey.

## Requirements

- Windows (x64)
- [RivaTuner Statistics Server](https://www.guru3d.com/download/rtss-rivatuner-statistics-server-download/) installed separately

RTSS handles the in-game display. It is not included in this repository.

## Building

AppVeyor uses the included `appveyor.yml` to publish a Windows x64 build. The ZIP is available in the build's Artifacts tab after a successful build.

To build locally, install the [.NET 8 SDK](https://dotnet.microsoft.com/en-us/download/dotnet/8.0) and run `Build.cmd`.

More detailed setup instructions will be added after the first round of testing.

## Bugs and suggestions

Found a problem or have an idea? [Open an issue](https://github.com/LSVJ-Coding/GameOverlay/issues/new/choose).

Please check existing issues before creating a new one. Bug reports are especially helpful when they include the Windows and RTSS versions and the game where the problem occurred.

## License

Copyright (c) 2026 LSVJ-Coding.

The original project code is licensed under [GPL-3.0-only](LICENSE). External libraries retain their own licenses. See [third-party notices](THIRD-PARTY-NOTICES.md) for details.

LSVJ Overlay is an independent project and is not affiliated with Razer, MSI, or the developers of RTSS.
