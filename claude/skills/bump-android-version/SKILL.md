---
name: bump-android-version
description: Bump VERSION_CODE and VERSION_NAME in app.env for an Android release. Use when the user wants to release a new Android version. Pass the new version name as argument (e.g. "1.0.0-rc11"); version code is auto-incremented.
---

## Bump Android version

Android version metadata lives in `app.env`:

```
VERSION_NAME=1.0.0-rc10
VERSION_CODE=9
```

Both must be bumped together for Play Store / sideload upgrades to work — `VERSION_CODE` is an integer that must strictly increase, `VERSION_NAME` is the user-visible string. CMake reads them through `app_load_env()` and the build scripts source the same file, so there is no second copy to keep in sync.

### Steps

1. Read current values from `app.env` — the `VERSION_CODE=` and `VERSION_NAME=` lines.
2. Increment `VERSION_CODE` by 1.
3. Set `VERSION_NAME` to `$ARGUMENTS` if provided. If empty, ask the user what name to use — do not invent one.
4. Apply both edits with the Edit tool.
5. Show the user the diff and ask for confirmation before they build.

### Don't

- Do not bump only one of the two values.
- Do not skip a `VERSION_CODE` (e.g. 9 → 11) without telling the user.
- Do not commit the change automatically — leave that to the user.
- Do not trigger a build immediately after — the user may want to verify the bump first.
- Do not use this skill for an official release build: `libs/scripts/build-android.sh … official` bumps, commits and tags on its own.
