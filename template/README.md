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

## Layout

| Path | Role |
|------|------|
| `app.env` | Every app parameter: name, target, version, Android package, keystore, AppImage metadata. CMake reads it through `app_load_env()`, the scripts source it |
| `CMakeLists.txt` | Composed from `libs/cmake/AppProject.cmake` helpers |
| `src/main.cpp` | Entry point: engine, `QmlRegistrator`, optional automation server |
| `src/ui/qml/` | The QML module, URI `__APP_QML_URI__` |
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

`App.Components` items `AppView` and `BackHandler` work as-is. Every other component in `App.Components` and all of `Themed.Components` read a `Theme` singleton that the **app** provides:

```cpp
registrator.registerSingletonType("Themed.Components", "Theme.qml", "Theme");
```

Add a `Theme.qml` to your QML module with the colors, spacing and control styles those components expect before importing them.
