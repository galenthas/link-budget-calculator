# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.3] - 2026-04-14

### Added
- Initial public release
- Win32 dark-theme GUI with 8 input sections
- Physics engine with ITU-R P.676-12 (gaseous) and P.838-3 (rain) atmospheric models
- Antenna modes: direct gain input or parabolic dish (auto gain + beamwidth)
- Geometry modes: slant range or horizontal distance
- Path loss modes: Simple FSPL or ITU-R
- Clickable unit cycling for frequency, altitude, and power fields
- Sweep plot with zero-crossing detection and interactive crosshair
- Save/Load scenario files (.lbf)
- CSV export and direct PDF 1.4 report generation
- 133 unit tests covering all public API functions
- Version header (version.h) for single-source version management
- GitHub Actions CI workflow (Windows/MinGW-w64)
