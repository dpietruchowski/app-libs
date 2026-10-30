# Android system bars and insets

How an app on these libs handles the status bar, the navigation bar and the keyboard on
Android, and why. Most of this was reached by trial and error; the "Don't" list records what
already failed, so it is not tried again.

## How it works

- **Qt owns the bars.** The app does not colour the bars or toggle light/dark bar icons. Qt
  6.10 makes the window edge-to-edge only on API 35+. Below that, the window sits between the
  bars and the system draws the bars itself.
- **`AppView`** (`qml/app/AppView.qml`) is the root item. It fills the whole window with
  `systemBarColor` and places the content inside the `SafeArea` margins. When the window is
  edge-to-edge, that colour shows behind the bars. When it is not, the margins are 0 and the
  content fills the window. Read `SafeArea` null-safely (`root.SafeArea?.margins.top ?? 0`):
  under Qt 6.10 it is not always attached.
- **`systemBarColor` follows the system dark mode, not the in-app theme.** The bar icons
  follow the system colour scheme, so a strip coloured from the app theme can leave
  light icons on a light strip. `SystemBarStyler.systemDark` (palette based) picks the
  colour, e.g. `SystemBars.systemDark ? backgroundDark : backgroundLight`.
- **Popups and panels** inset themselves by the safe area as well (`ThemedPanel`, the
  `ThemedComboBox` popup margins).
- **Keyboard:** the activity uses `windowSoftInputMode="adjustNothing"`, so the bottom
  navigation stays at the bottom and the keyboard covers it. `KeyboardInsetProvider` reports
  the keyboard height (the native IME inset on API 30+, read asynchronously to avoid a
  GUI-thread deadlock and debounced). The navigation-bar inset is subtracted from it, and
  pages move their content by that value.
- **Activity theme:** setting a custom theme (needed for the splash `windowBackground`)
  stops Qt from applying its DayNight default. `values/` alone falls back to
  `Theme.Holo.Light`, which on Android 10 draws a black status bar with dark icons and hides
  the clock. Keep `values-v29/` on `Theme.DeviceDefault.DayNight` and let `values-v31/`
  carry its own splash style.

## Don't

- **Don't style the bars from the app** (`setStatusBarColor`, light-status-bar flags over
  JNI). This fought Qt and left the bar icons invisible. It was removed from `SystemBarStyler`,
  which now only detects system dark mode.
- **Don't force edge-to-edge below API 35** with `Qt.ExpandedClientAreaHint`, translucent
  status/navigation flags or `setSystemUiVisibility`. This was tried again for the splash and
  broke the status bar. A problem that looks like it needs the window behind the bars (for
  example the splash logo offset, see [android-splash.md](android-splash.md)) is solved
  another way.
- **Don't use `adjustResize`/`adjustPan`.** They move or shrink the whole window, including the
  bottom navigation. The keyboard height goes through `KeyboardInsetProvider` instead.
