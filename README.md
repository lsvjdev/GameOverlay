# LSVJ Performance Overlay

A lightweight, experimental Windows performance overlay using RTSS to render in compatible fullscreen games.

Features include FPS, clock, game-session playtime, CPU/GPU metrics, RAM usage, an in-app preview, tray controls, and Ctrl + Shift + O toggling.

## Build with AppVeyor

This repository includes `appveyor.yml` configured to build a self-contained Windows x64 executable on an AppVeyor Windows worker. Connect this public repository to AppVeyor, trigger a build, and download `LSVJOverlay-win-x64.zip` from the build's Artifacts tab. No GitHub Actions or Codemagic required.

RTSS must be installed separately to display the overlay in games. The in-game appearance depends on RTSS's styling, and some anti-cheat systems block its rendering. This is a **prototype and has not yet been compiled or tested on a Windows worker**.

Download RTSS: https://www.guru3d.com/download/rtss-rivatuner-statistics-server-download/
