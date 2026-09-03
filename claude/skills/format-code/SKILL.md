---
name: format-code
description: Run clang-format over the app sources via the CMake `format` target. Use when the user wants to format code, fix style, or check formatting before commit. Pass "check" to dry-run (fails on diff) instead of applying changes.
allowed-tools: Bash(cmake*)
---

## Format C++ sources

`app_project_setup()` (from `libs/cmake/AppProject.cmake`) registers two custom targets when `clang-format` is on PATH, covering the directories the root `CMakeLists.txt` passed as `FORMAT_DIRS`:

- `format` — apply formatting in-place
- `format_check` — dry-run, fails with diff (use in CI / pre-commit)

Style configuration: `libs/.clang-format`, with the repo-root `.clang-format` a symlink to it.

### Apply formatting

```bash
cmake --build build --target format
```

### Check only (no writes)

```bash
cmake --build build --target format_check
```

Use whichever build directory the project actually configures (`build`, `build-desktop`, …); check with `ls -d build*` if unsure.

### Behavior based on arguments

- If `$ARGUMENTS` contains "check" → run `format_check`.
- Otherwise → run `format`.

### Prerequisites

- A configured build directory (run `cmake -B build` first if missing).
- `clang-format` installed and on PATH. If absent, the targets won't be registered — re-configure CMake after installing.
