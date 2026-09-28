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

Stages 1 and 2 draw only the background colour; the logo exists only in QML, which fades
it in at its final position. The same on every API level and on desktop.

- One drawable, `splash.xml`: a `layer-list` with just the background colour. Not a bare
  `@color` — with a colour, MIUI draws the starting window black.
- Use it for `android:windowBackground` in `values/`, `values-v29/` and `values-v31/`, and
  for the manifest `android.app.splash_screen_drawable`.
- **API 31+:** the SplashScreen API always shows the starting window with an icon, but the
  icon is any drawable. Set `android:windowSplashScreenBackground` to the same colour and
  `android:windowSplashScreenAnimatedIcon` to a transparent shape. Without that attribute
  the system falls back to the launcher icon. A blank splash icon is allowed on Google Play.
- The QML splash starts with the logo at opacity 0, fades it in, and dismisses itself only
  after the fade-in has finished, so a fast start does not flash the logo.
- Hide the Qt splash (`QAndroidApplication::hideSplashScreen(0)`) on the first
  `QQuickWindow::frameSwapped`, not earlier. Otherwise a frame of the bare window shows
  between stage 2 and stage 3.

## Rejected

- `android:windowSplashscreenContent` (API 26–30): MIUI renders it on black.
- A hard-coded offset in the drawables: it depends on the device's bar heights.
- Keeping the system logo on API 31+ (an `inset` logo matched to the QML size): it cannot
  be combined with the fade-in, because the logo would vanish and come back.

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
