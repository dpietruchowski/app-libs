---
name: ui-session
description: Keep the app open as ONE persistent instance for UI automation, then drive it by objectName without relaunching it each time. Use when testing or driving the app across many steps (dump, find, click, set, invoke, screenshot). Pass a natural-language instruction or a raw driver command (e.g. "start", "stop", "status", "click primaryButton", "find settings").
allowed-tools: Bash(python3 libs/tools/ui_session.py*), Bash(python3 libs/tools/ui_driver.py*)
---

## Drive the running app

Two scripts under `libs/tools/`, both configured by `.ui_automation.json` in the repo root (binary paths, port, port env var):

- `ui_session.py` — lifecycle only: `start` / `status` / `stop` / `restart` / `logs`.
- `ui_driver.py` — sends one command to the running instance over TCP and prints the result.

> Do NOT use `ui_driver.py shell` or `ui_driver.py launch` for stepwise work — both tie the app's lifetime to that one process and kill it on exit.

### Start once

```bash
python3 libs/tools/ui_session.py start
python3 libs/tools/ui_session.py start --platform offscreen   # headless run
```

Session state and the app log live in `tmp/ui_session/` inside the repo, so nothing lands in a system temp directory. Pass `--platform` instead of exporting `QT_QPA_PLATFORM`, so the command stays a single simple call.

When `.ui_automation.json` carries a `sandbox` section, the app runs in that directory (seeded with copies of the listed files, with the listed env vars pointing inside it) instead of the repo root, so driving it never touches the real user data. `--run-dir PATH` picks a different one.

It checks the configured port first: if the app already answers there it adopts that instance and does nothing. Otherwise it launches the configured Debug binary detached and waits until it responds. Idempotent, so run it before every driving session.

### Then drive it

```bash
python3 libs/tools/ui_driver.py ping
python3 libs/tools/ui_driver.py find ""             # every reachable objectName
python3 libs/tools/ui_driver.py find "settings"     # filter by substring
python3 libs/tools/ui_driver.py dump                # full tree with properties
python3 libs/tools/ui_driver.py get someLabel text
python3 libs/tools/ui_driver.py set someInput text "hello"
python3 libs/tools/ui_driver.py click someButton
python3 libs/tools/ui_driver.py tap 200 400
python3 libs/tools/ui_driver.py key someInput "Return"
python3 libs/tools/ui_driver.py scroll someListView 300
python3 libs/tools/ui_driver.py scrollto someItem
python3 libs/tools/ui_driver.py invoke someViewModel someMethod arg1
python3 libs/tools/ui_driver.py screenshot /tmp/app.png
```

Clock control, when the app injects a `TimeProvider` (`libs/cpp/async`):

```bash
python3 libs/tools/ui_driver.py get_time
python3 libs/tools/ui_driver.py set_time 2026-10-01
python3 libs/tools/ui_driver.py advance_time 3
python3 libs/tools/ui_driver.py reset_time
```

### Rules

- One driver command per Bash call — chaining with `;` or `&&`, or a `D=...` variable, turns it into a shape the permission rules cannot match. For a longer sequence write `tmp/drive.py`, import `ui_driver`, and run it once.
- Only objects with an `objectName` are reachable. If a control cannot be found, add the `objectName` in QML, rebuild, and `ui_session.py restart`.
- A modal dialog blocks everything behind it — close it before driving the page below.
- After changing C++ or QML, rebuild and `restart`; a running instance keeps the old code.
- Stop the instance when finished: `python3 libs/tools/ui_session.py stop`.
- Read the app's output with `python3 libs/tools/ui_session.py logs -n 40` (or open `tmp/ui_session/app.log`).

### Requirements

The build must have the automation server compiled in — `LIBS_AUTOMATION` is ON for Debug builds by default. A Release build has no server, so `start` will time out against it.
