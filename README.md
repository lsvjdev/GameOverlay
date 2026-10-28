# Game Overlay

Game Overlay is a small Windows performance overlay that uses RivaTuner Statistics Server (RTSS) to display system stats in games.

The goal is a clean, compact overlay without a separate game booster or a large background app.

## Current status

**Early prototype.** The app has been built and launched on Windows, but the in-game RTSS integration still needs testing. Some features may not work as expected, and RTSS support depends on the game. There is no official installer or stable release yet.

The overlay shows FPS, a 12/24-hour clock, playtime, CPU and GPU readings, and RAM usage. The optional desktop strip can be placed at the top left, center, or right. Use `Ctrl + Shift + O` to toggle the in-game display.

## Requirements

- Windows (x64)
- [RivaTuner Statistics Server](https://www.guru3d.com/download/rtss-rivatuner-statistics-server-download/) installed separately

RTSS handles the in-game display. It is not included in this repository.

## Building

AppVeyor produces two Windows x64 builds. Download either ZIP from the build's Artifacts tab:

- `GameOverlay-standalone-win-x64.zip` includes the .NET runtime and works without installing it separately. The file is much larger.
- `GameOverlay-small-win-x64.zip` needs the [.NET 8 Desktop Runtime](https://dotnet.microsoft.com/en-us/download/dotnet/8.0) installed on the PC, but takes less disk space.

Both builds need RTSS separately for in-game rendering. The 12/24-hour clock and desktop preview position are saved in your local settings.

To build locally, install the [.NET 8 SDK](https://dotnet.microsoft.com/en-us/download/dotnet/8.0) and run `Build.cmd`.

More detailed setup instructions will be added after the first round of testing.

## Bugs and suggestions

Found a problem or have an idea? [Open an issue](https://github.com/LSVJ-Coding/GameOverlay/issues/new/choose).

Please check existing issues before creating a new one. Bug reports are especially helpful when they include the Windows and RTSS versions and the game where the problem occurred.

## License

Copyright (c) 2026 LSVJ-Coding.

The original project code is licensed under [GPL-3.0-only](LICENSE). External libraries retain their own licenses. See [third-party notices](THIRD-PARTY-NOTICES.md) for details.

Game Overlay is an independent project and is not affiliated with Razer, MSI, or the developers of RTSS.
