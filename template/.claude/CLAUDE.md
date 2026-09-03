# __APP_NAME__ — context for the LLM

Qt6/QML application built on the `libs` submodule. Builds on Linux (AppImage) and Android (APK).

**Stack**: C++20, Qt 6 (Core/Gui/Qml/Quick), CMake + Ninja, GoogleTest.

## Layout

```
src/main.cpp        Entry point: engine, QmlRegistrator, singletons, automation server
src/ui/qml/         QML module, URI __APP_QML_URI__ — Main.qml, Theme.qml
libs/cpp            async, utils, platform, qmlutils, dbtoolkit, eventbus, agent
libs/qml            themed (Themed.Components), app (App.Components), icons
```

`libs/cpp` is the include root, so headers are reached by concern: `#include "async/task.h"`,
`#include "platform/securestorage.h"`, `#include "dbtoolkit/dbtoolkit.h"`. Library targets are
named `app_<directory>` (`app_async`, `app_platform`, `app_qmlutils`, `app_dbtoolkit`, …).

## Build

Every app parameter (name, target, version, Android package, keystore, AppImage metadata) lives
in `app.env`. CMake reads it through `app_load_env()`, the scripts source the same file — there is
no second copy to keep in sync.

```bash
cmake -B build-desktop -DCMAKE_BUILD_TYPE=Debug
cmake --build build-desktop -j$(nproc)
./build-desktop/src/app__APP_ID__

./libs/scripts/build-android.sh debug apk
./libs/scripts/build-appimage.sh
```

The root `CMakeLists.txt` is composed from the helpers in `libs/cmake/AppProject.cmake`:
`app_project_setup`, `app_add_module`, `app_add_qml_module`, `app_add_test`, `app_configure_android`.
Formatting runs through the `format` / `format_check` targets; the style is `libs/.clang-format`
and the repo-root `.clang-format` is a symlink to it.

**New source or QML file → re-run `cmake -B build-desktop`.** The helpers glob sources at configure
time, so a new file is invisible (and AUTOMOC silently skips it) until the tree is reconfigured.

## Theming

`Themed.Components` and most of `App.Components` read a `Theme` singleton that the app registers:

```cpp
registrator.registerSingletonType("Themed.Components", "Theme.qml", "Theme");
```

`src/ui/qml/Theme.qml` derives from `DefaultTheme` (shipped by `libs/qml/themed`), which carries the
whole contract those components expect — colors, fontSize, spacing, padding, radius, border,
elevation, text/page styles, button sizes and variants, list/scroll/slider/busy/empty groups, icons.
Override only what differs:

```qml
pragma Singleton
import Themed.Components

DefaultTheme {
    isNightMode: true
}
```

## UI automation

Debug builds link `app_automation`, an in-process TCP server that drives the running app by
`objectName`. `.ui_automation.json` in the repo root tells the drivers which binary, port and env
var to use (`APP_AUTOMATION_PORT`, port 49200 by default).

```bash
python3 libs/tools/ui_session.py start      # launch (or adopt) one persistent instance
python3 libs/tools/ui_driver.py find ""     # list reachable objectNames
python3 libs/tools/ui_driver.py click primaryButton
python3 libs/tools/ui_driver.py get greetingText text
python3 libs/tools/ui_driver.py screenshot /tmp/app.png
python3 libs/tools/ui_session.py stop
```

Give every interactive QML control an `objectName` — an item without one is invisible to the driver.

## Conventions

- **No comments in code** (C++ and QML). The code must be self-explanatory: name things properly
  instead of explaining them.
- **Commit messages**: `<Verb> <Subject>[: <details>]`, English, imperative, capitalized, no trailing
  period. No Conventional Commits prefixes. **Never add a `Co-Authored-By` or any other attribution
  trailer.**
- **Stage explicitly**: commit with `git commit -- <paths>`, never `git add -A` / `git add .`.
- **`libs` is a submodule**: when it changed, commit inside `libs` first, then commit the app together
  with the new submodule pointer. A pointer bump without the matching libs commit breaks everyone's
  build.
- **Never bump the version by hand** — `./libs/scripts/build-android.sh … official` bumps `app.env`,
  commits and tags on its own.
- **Async work off the GUI thread**: services derive from `Service` (`libs/cpp/async`) and run on a
  `BackendWorker` thread, returning `Task<T>`; view models continue with `.then(this, …)` to get back
  onto the GUI thread. `Result<T>` is returned across boundaries instead of throwing.
