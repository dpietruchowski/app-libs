# app-libs

The shared foundation for Qt6/QML apps. Everything that would otherwise be copied from one app to the next lives here: C++ libraries, QML components and theming, CMake helpers, Android and AppImage build scripts, UI automation tools, and the Claude Code skills used during development.

An app adds it as a git submodule at `libs/` and keeps only its own domain code. [qanvas](https://github.com/dpietruchowski/qanvas) is built this way.

## What's inside

- **C++ libraries** (`cpp/`): async, event bus, database toolkit, agent, automation, crash handler, platform and QML utilities. Each directory is a CMake target named `app_<directory>`: `app_async`, `app_platform`, `app_qmlutils`, `app_utils`, `app_dbtoolkit`, `app_eventbus`, `app_agent`, `app_automation`, `app_crashhandler`, `app_qmllive`. `cpp/` is the include root, so headers are included as `async/task.h`, `dbtoolkit/dbtoolkit.h` and so on.
- **QML modules** (`qml/`): the theme (`Themed.Theme`, with the `DefaultTheme` base), themed components (`Themed.Components`), app components (`App.Components`) and icons.
- **CMake helpers** (`cmake/AppProject.cmake`): `app_load_env`, `app_project_setup`, `app_add_module`, `app_add_qml_module`, `app_add_test`, `app_configure_android`.
- **Build scripts** (`scripts/`): `build-android.sh`, `build-appimage.sh`, `sign-apk.sh`, `sign-aab.sh`, `generate-keystore.sh` and `new-app.sh`. All of them read their settings from the app's `app.env`. Android builds run in the Docker image from `docker/`.
- **UI automation** (`tools/`): `ui_driver.py` and `ui_session.py` drive a running app by `objectName`.
- **Claude Code skills and agents** (`claude/`), which each app symlinks into its own `.claude/`.
- **Notes** (`doc/`) on the parts that need more than a line of explanation:
  - [`read-port.md`](doc/read-port.md): reading a joined shape into a typed row
  - [`android-splash.md`](doc/android-splash.md): Android splash stages without a jumping logo
  - [`android-system-bars.md`](doc/android-system-bars.md): system bars, safe area and keyboard insets, and what not to try
- `template/` is the skeleton used by `new-app.sh`, and `tests/` holds the unit tests for the libs.

## Starting a new app

```bash
mkdir myapp && cd myapp && git init
git submodule add git@github.com:dpietruchowski/app-libs.git libs
git submodule update --init --recursive
./libs/scripts/new-app.sh MyApp
cmake -B build-desktop -DCMAKE_BUILD_TYPE=Debug
cmake --build build-desktop -j$(nproc)
./build-desktop/src/appmyapp
```

`new-app.sh` creates `app.env`, the CMake tree, a minimal `main.cpp` and QML module, the `.clang-format` symlink and the `.claude` skill links. It never overwrites existing files, so you can re-run it any time to pick up files added to the template later.

## Build options

| Option | Default | Effect |
|--------|---------|--------|
| `LIBS_BUILD_TESTS` | ON standalone, OFF as a submodule | Build the unit tests for the libs |
| `LIBS_AUTOMATION` | ON for Debug | Build `app_automation` and define `LIBS_AUTOMATION` for the app |
| `QML_LIVE_ENABLED` | OFF (always OFF on Android) | Load QML from the source tree, see below |

## Live QML editing

**Source reload.** With `QML_LIVE_ENABLED` on, the app loads QML from the source tree instead of the compiled resources, so after editing a `.qml` file you just restart the app. No rebuild needed. Map each module to its directory with `app_qml_live_map(<target> URI dir …)` and call `QmlRegistrator::enableSourceReload(APP_QML_LIVE_MAP)`.

**Sandbox.** `app_qml_live_map()` also defines `APP_QML_SANDBOX_DIR` (`<source root>/sandbox`). Pass it to `QmlRegistrator::enableSandbox(<module uri>, APP_QML_SANDBOX_DIR)` and the module gets a `QmlSandbox` singleton. When the `APP_QML_SANDBOX` environment variable is set, the app should load `SandboxHost` (from `libs/qml/app`) instead of its normal root.

`SandboxHost` lists the `.qml` files in the sandbox and opens the one you pick by file path, so experiments never have to be module types and never need a rebuild. It also watches the sandbox and every mapped QML source tree and clears the component cache on each change, so edits to real app components show up without a restart. The only exception is QML singletons such as `Theme.qml`. The cache doesn't re-create them, so theme changes still need a restart.
