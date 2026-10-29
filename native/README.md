# Native development build

Game Overlay is a native Windows C++ performance overlay. This branch is
experimental; the native build currently supports PresentMon FPS tracking,
CPU/RAM monitoring and optional GPU/CPU sensor backends.

## Portable package

Keep the executable and the `bin` directory together:

```text
GameOverlayDev.exe
bin/
  PresentMon-2.4.1-x64.exe
  modules/AMDFamily17.bin
  licenses/
    LICENSE
    THIRD-PARTY-NOTICES.md
    PresentMon-LICENSE.txt
    PawnIO-Modules-LICENSE.txt
    PawnIO-Modules-SOURCE.txt
```

Launch `GameOverlayDev.exe`. It opens in the system tray. Double-click
the tray icon or right-click and choose **Open settings**.

The **About** page links to the GitHub repository and displays the
license and source information included in the portable distribution.
Settings are stored locally at
`%LOCALAPPDATA%\\GameOverlay\\settings.ini`; performance data is not saved.

## Shortcuts

- `Ctrl+Shift+O`: show/hide overlay
- `Ctrl+Shift+D`: copy in-memory diagnostics
- `Ctrl+Shift+Q`: exit

## Dependencies

- PresentMon is required for FPS and must stay in `bin`.
- NVIDIA driver NVML is used for NVIDIA GPU readings when supported.
- AMD CPU temperature requires separately installed PawnIO and the
  `AMDFamily17.bin` module placed in `bin/modules`.
- CPU/RAM usage and the clock work without those optional sensors.
- The native executable uses Windows APIs, not .NET.

CPU temperature and GPU availability differ across devices. Unsupported
readings are not fabricated; the app leaves them out or shows `--`.
