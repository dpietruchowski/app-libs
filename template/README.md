# __APP_NAME__

A Qt6/QML application built on the `libs` submodule.

## Build

```bash
cmake -B build-desktop -DCMAKE_BUILD_TYPE=Debug
cmake --build build-desktop -j$(nproc)
./build-desktop/src/app__APP_ID__
```

Android APK and the Linux AppImage come from the shared scripts, which read every app parameter from `app.env`:

```bash
./libs/scripts/build-android.sh debug apk
./libs/scripts/build-appimage.sh
```

## Tests

`tests/` holds the app's GoogleTest suite, wired with `app_add_test`. It is off unless asked for, and it needs the libs' bundled GoogleTest, so init the submodule recursively:

```bash
git submodule update --init --recursive
cmake -B build-desktop -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build-desktop -j$(nproc)
cd build-desktop && ctest
```

## Layout

| Path | Role |
|------|------|
| `app.env` | Every app parameter: name, target, version, Android package, keystore, AppImage metadata. CMake reads it through `app_load_env()`, the scripts source it |
| `CMakeLists.txt` | Composed from `libs/cmake/AppProject.cmake` helpers |
| `src/main.cpp` | Entry point: engine, `QmlRegistrator`, optional automation server |
| `src/ui/qml/` | The QML module, URI `__APP_QML_URI__` |
| `tests/` | GoogleTest suite, built when `BUILD_TESTING=ON` |
| `libs/` | The shared submodule — C++ under `libs/cpp/`, QML under `libs/qml/`, tooling under `libs/scripts/` and `libs/tools/` |

## UI automation

Debug builds link `app_automation`, an in-process TCP server that drives the app by `objectName`:

```bash
python3 libs/tools/ui_session.py start
python3 libs/tools/ui_driver.py click primaryButton
python3 libs/tools/ui_driver.py get greetingText text
python3 libs/tools/ui_session.py stop
```

`.ui_automation.json` tells the drivers which binary, port and env var this repo uses.

## Using Themed.Components

Every component in `Themed.Components`, and most of `App.Components`, reads a `Theme` singleton that the **app** provides. `new-app.sh` already wires it: `src/ui/qml/Theme.qml` derives from `DefaultTheme` (module `Themed.Theme`) and `main.cpp` registers it:

```cpp
registrator.registerSingletonType("Themed.Components", "Theme.qml", "Theme");
```

Override only the knobs that differ (`isNightMode`, the palette, spacing) instead of restating the whole contract.
