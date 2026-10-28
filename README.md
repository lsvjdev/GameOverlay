# LSVJ Performance Overlay

**Experimental lightweight Windows performance overlay**, using RTSS to render a
Razer-style metrics strip in compatible fullscreen games.

Features: FPS, local clock, game-session timer, CPU/GPU metrics, RAM usage,
desktop preview, tray controls, and Ctrl + Shift + O toggling.

> Prototype status: the project has **not yet been compiled and tested on a
> Windows worker**, and some games/anti-cheat systems do not support RTSS.
> The displayed in-game fonts and background are partly controlled by RTSS.

## Building with AppVeyor

The checked-in **appveyor.yml** builds a self-contained Windows x64 app on
AppVeyor's Windows worker, with no GitHub Actions or Codemagic required.

1. Connect this public repository to AppVeyor and start a build.
2. Download **LSVJOverlay-win-x64.zip** from **Artifacts**.
3. Extract all files, keep any bundled notices/licenses, and launch
   **LSVJOverlay.exe**. Install RTSS separately first.

For a local build with the .NET 8 SDK, use **Build.cmd**.

[Download RTSS independently](https://www.guru3d.com/download/rtss-rivatuner-statistics-server-download/).

## License & ownership

**Copyright © 2026 LSVJ-Coding.** Original application source code is provided
under **GNU GPL-3.0-only**. See [LICENSE](LICENSE).

This is genuine open-source software: you may use, study, modify, and
redistribute it under GPLv3's terms, including commercially. Modifications
distributed as a covered work must comply with GPLv3; original copyright
ownership remains with the original rightsholders.

- [Third-party notices](THIRD-PARTY-NOTICES.md) — library credits, distinct
  license terms, RTSS separation, and .NET Windows licensing.
- [LICENSES](LICENSES) — preserved third-party license texts and original
  LibreHardwareMonitor third-party notices.

Before making an official release or installer, verify that the actual
published binaries have the required notices/permissions and provide
**corresponding source for that exact release**, ideally using a tagged Git
commit. Don't delete the source repository while public binary distributions
still rely on it for source access.

RTSS, its installer, and drivers are **not** redistributed by this project.
This project is independent of Razer and the other upstream software vendors.
