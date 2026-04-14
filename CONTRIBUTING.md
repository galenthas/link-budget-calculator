# Contributing

## Prerequisites

- [MSYS2](https://www.msys2.org/) with MinGW-w64: `pacman -S mingw-w64-x86_64-gcc`

## Build

```bash
# Compile resource file
PATH="/c/msys64/mingw64/bin:$PATH" windres app.rc -O coff -o app.res

# Build GUI
PATH="/c/msys64/mingw64/bin:$PATH" gcc -Wall -Wextra -std=c11 -O2 \
    linkbudget_gui.c linkbudget_ui_controls.c linkbudget_ui_logic.c \
    linkbudget_ui_io.c linkbudget_pdf.c linkbudget_core.c app.res \
    -o "Link Budget Calculator" -lcomctl32 -lcomdlg32 -mwindows -lm
```

## Run Tests

```bash
PATH="/c/msys64/mingw64/bin:$PATH" gcc -Wall -Wextra -std=c11 -O2 \
    linkbudget_core.c test_linkbudget.c -o test_linkbudget -lm
./test_linkbudget
```

All 133 tests must pass before submitting a PR.

## Versioning

Bump `version.h` for any user-visible change. Follow [Semantic Versioning](https://semver.org):
- `PATCH` — bug fixes
- `MINOR` — new features, backwards compatible
- `MAJOR` — breaking changes

Update `CHANGELOG.md` with a new entry for the version.

## Submitting a PR

1. Fork the repo and create a branch from `master`
2. Make your changes
3. Ensure all tests pass
4. Update `CHANGELOG.md` and `version.h` if applicable
5. Open a pull request — CI must be green before merging
