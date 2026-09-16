# Development

[中文](development_zh.md) · [Home](../README.md)

## Development Requirements

Install Qt6 (Core and Widgets) and Ninja, then set `SURFPANEL_QT_ROOT` to the
Qt installation prefix—the directory that contains `lib/cmake/Qt6/Qt6Config.cmake`.
CMake deliberately stops with a clear error when this variable is absent or
invalid.

PowerShell, for the current terminal:

```powershell
$env:SURFPANEL_QT_ROOT = "C:\Qt\6.8.0\mingw_64"
```

To persist it for future terminals on Windows:

```powershell
[Environment]::SetEnvironmentVariable("SURFPANEL_QT_ROOT", "C:\Qt\6.8.0\mingw_64", "User")
```

To build the Windows installer, also set `SURFPANEL_MINGW_ROOT` to the MinGW
prefix containing `bin/libstdc++-6.dll`. `pack.bat` uses the standard Inno
Setup 6 location by default; set `SURFPANEL_ISCC` to the full path of
`ISCC.exe` when it is installed elsewhere.

The installer performs in-place upgrades using the stable Inno Setup `AppId`.
If SurfPanel is running, Windows Restart Manager lists it on the Preparing to
Install page and offers to close it automatically before files are replaced.
After an upgrade, Restart Manager starts the updated daemon again; on a fresh
install, the Finish page offers to launch it.

## Palette Appearance

On Windows 11 22H2 and later, the palette uses the system Desktop Acrylic
backdrop, small rounded corners, and DWM framing/shadow. Older Windows versions
and unavailable native effects use a simple translucent panel and thin border.
No wallpaper sampling or Qt drop-shadow effect is used.

The palette is 660 logical pixels wide and opens on the screen containing the
cursor, horizontally centered with its top edge around 35% of the available
screen height. Height follows the results, displaying up to six 44-pixel rows
before scrolling; empty results show an informational, non-actionable row.
Type icons and `Link`, `Snippet`, and `Plugin` labels identify actions.

Light/dark appearance follows Qt's system color scheme, with a compatibility
fallback for older Qt versions. The Windows accent marks search focus and the
selected row. A 90 ms, 4-pixel opening animation follows the Windows animation
setting and never delays keyboard input.

For UI verification, set `SURFPANEL_UI_CAPTURE_DIR` to a build-artifact directory
and run `test_mainwindow` through CTest. It exports light/dark native and fallback
previews; normal test runs do not capture the desktop.

The same opt-in exports `readme-light.png`, `readme-dark.png`, and `tray.png`.
The README palette previews use widget capture with the fallback surface so they
can be reproduced without a capturable desktop; native DWM effects are excluded.
Copy the reviewed previews to `assets/SurfPanel.png`, `assets/SurfPanel-dark.png`,
and `assets/tray.png` when updating the README images.

## Build and Test

Run these commands from the repository root after configuring the environment:

```powershell
git submodule update --init --recursive
make cg-debug
make build
make test
```

For a release installer, configure with `make cg-release`, build with `make build`,
then run `make pack`. Use a C++17 compiler matching the Qt toolchain, CMake, Ninja,
and GNU Make. Keep generated `build/` and `Output/` files out of commits.

See [AGENTS.md](../AGENTS.md) for contributor and plugin conventions.
