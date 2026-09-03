# app-libs

Shared foundation for Qt6/QML applications: C++ libraries, QML modules, the CMake
composition helpers, the build and signing scripts, the UI-automation tooling, and
the Claude skills that are the same in every app.

An app consumes it as a submodule at `libs/` and keeps only its own domain code.

## Layout

| Path | Contents |
|------|----------|
| `cpp/` | C++ libraries; this directory is the include root, so headers are reached as `async/task.h`, `dbtoolkit/dbtoolkit.h`, … |
| `qml/` | QML modules: `theme` (URI `Themed.Theme`, the `DefaultTheme` base), `themed` (URI `Themed.Components`), `app` (URI `App.Components`), `icons` |
| `cmake/` | `AppProject.cmake` — `app_load_env`, `app_project_setup`, `app_add_module`, `app_add_qml_module`, `app_add_test`, `app_configure_android` |
| `scripts/` | `build-android.sh`, `build-appimage.sh`, `sign-apk.sh`, `sign-aab.sh`, `generate-keystore.sh`, `new-app.sh` — all parametrized by the consuming repo's `app.env` |
| `docker/` | The Android build image |
| `tools/` | `ui_driver.py` / `ui_session.py` — drive a running app by `objectName` |
| `claude/` | Skills and agents an app symlinks into its own `.claude/` |
| `template/` | The skeleton `new-app.sh` instantiates |
| `tests/` | Unit tests for the libs |

C++ targets are named `app_<directory>`: `app_async`, `app_platform`, `app_qmlutils`,
`app_utils`, `app_dbtoolkit`, `app_eventbus`, `app_agent`, `app_automation`,
`app_crashhandler`, `app_qmllive`.

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

`new-app.sh` writes `app.env`, the CMake tree, a minimal `main.cpp` and QML module, the
`.clang-format` symlink and the `.claude` skill links. It never overwrites an existing
file, so it is safe to re-run to pick up files added later.

## Options

| Option | Default | Effect |
|--------|---------|--------|
| `LIBS_BUILD_TESTS` | ON standalone, OFF as a submodule | Build the libs' own tests |
| `LIBS_AUTOMATION` | ON for Debug | Build `app_automation` and define `LIBS_AUTOMATION` for consumers |
| `QML_LIVE_ENABLED` | OFF (forced OFF on Android) | Load QML from the source tree instead of the compiled resource, via `app_qmllive` — edit a `.qml`, restart the app, no rebuild. Declare the module→directory map with `app_qml_live_map(<target> URI dir …)` and call `QmlRegistrator::enableSourceReload(APP_QML_LIVE_MAP)` |
