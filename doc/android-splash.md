# Android splash without a jumping logo

A Qt app on Android goes through three splash stages before the first QML frame is fully
in charge. If more than one of them draws the logo, the logo moves between them.

| Stage | Drawn by | Area it is centred in |
|-------|----------|-----------------------|
| 1. Starting window | the system, from the theme's `android:windowBackground` (API < 31) or the SplashScreen API (API 31+) | the **whole screen**, behind the status and navigation bars |
| 2. Qt splash | `QtActivityDelegate`, an `ImageView` from the manifest meta-data `android.app.splash_screen_drawable` (kept with `splash_screen_sticky`) | the **content area** between the bars |
| 3. QML splash | the app's own item over the root window | the **content area** between the bars (unless the window is edge-to-edge) |

Stage 1 and stage 2 are static resources shown before any app code runs, so they cannot
read the window insets. On API < 31 the difference between them is
`(statusBarHeight - navigationBarHeight) / 2` — on a Xiaomi Mi 9T (96 px status bar,
130 px navigation bar) the logo jumps 17 px up. Qt only goes edge-to-edge on API 35+;
below that, forcing it (`Qt.ExpandedClientAreaHint`, translucent bar flags) changes how
the app draws under the system bars, so it is not a fix for the splash (see
[android-system-bars.md](android-system-bars.md)).

## Recipe

Only one stage may show the logo before QML takes over.

- **API < 31:** stages 1 and 2 show just the background colour; QML fades the logo in at its
  final position.
  - Put the background in a `layer-list` drawable (`splash_plain.xml`), not a bare
    `@color`. With a colour, MIUI draws the starting window black.
  - Point `android:windowBackground` in `values/` and `values-v29/` at that drawable.
  - Point `android.app.splash_screen_drawable` at a resource with an API-qualified
    variant (`drawable/splash_start.xml` → plain, `drawable-v31/splash_start.xml` → with logo).
- **API 31+:** the system splash always draws an icon and cannot be made logo-less without
  a blank icon. Keep the logo in all three stages and make them match. Set
  `android:windowSplashScreenAnimatedIcon` to an `inset` drawable. With no icon
  background the canvas is 288 dp, so an 84 dp inset gives a 120 dp logo. Without that
  attribute the system falls back to the adaptive launcher icon on a 240 dp canvas.
  Stage 2 and QML then draw the same 120 dp logo, and the QML splash shows it without a
  fade.
- The app tells QML which case applies, e.g. a context property set from
  `QNativeInterface::QAndroidApplication::sdkVersion() < 31`. On desktop there is no
  native splash, so the fade-in is always fine there.
- The QML splash dismisses itself only after the fade-in has finished, so a fast start does
  not flash the logo.
- Hide the Qt splash (`QAndroidApplication::hideSplashScreen(0)`) on the first
  `QQuickWindow::frameSwapped`, not earlier. Otherwise a frame of the bare window shows
  between stage 2 and stage 3.

## Rejected

- `android:windowSplashscreenContent` (API 26–30): MIUI renders it on black.
- A hard-coded offset in the drawables: it depends on the device's bar heights.

## Verifying

Record the launch and measure the logo in each frame:

```bash
adb shell am force-stop <package>
adb shell screenrecord --time-limit 6 /sdcard/splash.mp4   # start, then launch with am start -n
adb pull /sdcard/splash.mp4
ffmpeg -i splash.mp4 frame_%04d.png
```

Then compute the logo's bounding box per frame (e.g. with PIL, as the set of pixels that
differ from the background colour). The centre must stay constant from the first frame
with a logo to the last, and the frames before it must have the background colour, not
black.
