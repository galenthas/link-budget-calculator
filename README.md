# Link Budget Calculator

A native Win32 RF/satellite link budget calculator with a dark-theme GUI, built in C.

![Tests](https://github.com/galenthas/link-budget-calculator/actions/workflows/test.yml/badge.svg)
![Platform](https://img.shields.io/badge/platform-Windows-blue)
![Language](https://img.shields.io/badge/language-C11-lightgrey)
![License](https://img.shields.io/badge/license-MIT-green)
![Version](https://img.shields.io/github/v/release/galenthas/link-budget-calculator)

## Screenshot

![Link Budget Calculator](screenshot.png)

## Installation

Download the latest release from the [Releases page](https://github.com/galenthas/link-budget-calculator/releases):

- **`LinkBudgetCalculator_vX.X.X_Setup.exe`** — Windows installer (no admin rights required)
- **`Link Budget Calculator.exe`** — portable, runs without installation

## Features

- **8-section input panel** — frequency, modulation/coding, geometry, transmitter, TX antenna, path loss, RX antenna, RX system noise
- **Antenna modes** — direct gain input or parabolic dish (auto-computes gain and beamwidth)
- **Geometry modes** — slant range or horizontal distance
- **Path loss models** — Simple FSPL or ITU-R (P.676-12 gaseous + P.838-3 rain attenuation)
- **Clickable unit cycling** — frequency (GHz/MHz/kHz), altitude (m/ft/km/NM), power (W/dBm/dBW)
- **Sweep plot** — margin vs. any parameter with zero-crossing detection and interactive crosshair
- **Save / Load** — `.lbf` scenario files
- **CSV export** — all inputs and computed results
- **PDF report** — direct PDF 1.4 output, no printer required
- **About dialog** — version info, physics model references, keyboard shortcuts

## Keyboard Shortcuts

| Key | Action |
|---|---|
| `F5` | Recalculate |
| `Ctrl+S` | Save scenario |
| `Ctrl+O` | Load scenario |
| `Ctrl+E` | Export CSV |
| `Ctrl+P` | PDF report |
| `Ctrl+W` | Sweep plot |

## Build

Requires [MSYS2](https://www.msys2.org/) with the MinGW-w64 toolchain.

```bash
# Install dependencies (once)
pacman -S mingw-w64-x86_64-gcc

# Build
PATH="/c/msys64/mingw64/bin:$PATH" windres app.rc -O coff -o app.res
PATH="/c/msys64/mingw64/bin:$PATH" gcc -Wall -Wextra -std=c11 -O2 \
    linkbudget_gui.c linkbudget_ui_controls.c linkbudget_ui_logic.c \
    linkbudget_ui_io.c linkbudget_pdf.c linkbudget_core.c app.res \
    -o "Link Budget Calculator" -lcomctl32 -lcomdlg32 -mwindows -lm

# Run tests
PATH="/c/msys64/mingw64/bin:$PATH" gcc -Wall -Wextra -std=c11 -O2 \
    linkbudget_core.c test_linkbudget.c -o test_linkbudget -lm
./test_linkbudget
```

## Project Structure

| File | Description |
|---|---|
| `linkbudget_core.c/h` | Physics engine — FSPL, EIRP, C/N0, Eb/N0, ITU-R models |
| `linkbudget_gui.c` | WinMain, WndProc, sweep window, tooltips, owner-draw buttons |
| `linkbudget_gui_config.h` | Layout constants, colours, control IDs |
| `linkbudget_ui_controls.c/h` | Section builders, control creators, panel WndProcs |
| `linkbudget_ui_logic.c/h` | Visibility, unit cycling, parameter extraction, calculation |
| `linkbudget_ui_io.c/h` | Save/load scenario, CSV export |
| `linkbudget_pdf.c/h` | PDF report generation |
| `test_linkbudget.c` | 141 unit tests |
| `version.h` | Single-source version definition |
| `installer.iss` | Inno Setup installer script |

## Versioning

Version is defined in `version.h`. To release a new version, update the three defines and tag:

```c
#define VERSION_MAJOR 1
#define VERSION_MINOR 1
#define VERSION_PATCH 1
```

```bash
git tag v1.1.1 && git push origin v1.1.1
```

CI will automatically build the installer and portable `.exe` and attach both to the GitHub Release.

## Physics Models

| Model | Standard |
|---|---|
| Free-Space Path Loss | Friis transmission equation |
| Oxygen attenuation | ITU-R P.676-12 Annex 2 |
| Water vapour attenuation | ITU-R P.676-12 Annex 2 |
| Rain attenuation | ITU-R P.838-3 |
| Parabolic antenna gain | G = η(πD/λ)² |
| Antenna beamwidth | θ = 67λ/D |
| System noise temperature | Cascade noise model (Friis) |
