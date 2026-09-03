#!/usr/bin/env python3
"""Ensure ONE persistent app instance is running for UI automation.

This is lifecycle only: it makes sure the app is up on its automation port
(launching it detached if needed), so you can then drive it with bare
ui_driver.py commands across separate shell invocations without the app
restarting each time.

Why: `ui_driver.py launch|shell` ties the app's lifetime to that one process and
kills it on exit. Here the app is launched detached with its port env var set;
its TCP server keeps listening, and every later `ui_driver.py <cmd>` opens its
own short-lived connection to it.

  ui_session.py start [--binary PATH] [--port N] [--platform offscreen]
  ui_session.py status                             # running? pid / port
  ui_session.py stop                               # kill the instance
  ui_session.py restart [--binary PATH] [--port N] [--platform NAME]
  ui_session.py logs [-n N]                        # tail the launched app log

`start` is idempotent: if the app already answers on the port (even one you
started by hand) it adopts it and does nothing.

Which binary and which port env var to use come from `.ui_automation.json` at
the consuming repo's root — see ui_driver.py. Session state and the app log go
to tmp/ui_session/ inside the repo, never to a system temp directory.

By default the child is started in the repo root, so any file it opens relative
to the working directory lands there. An app whose real user data sits in the
repo (a database next to the sources, a per-user directory under XDG_DATA_HOME)
can ask for a sandbox instead, so driving it never mutates that data:

    "sandbox": {
      "dir": "tmp/run",              # working directory, relative to the repo
      "copy": ["liftplanner.db"],    # seeded into it once, from the repo root
      "env": {"XDG_DATA_HOME": "data"}   # dirs created inside the sandbox
    }

`--run-dir PATH` picks a different sandbox for one run.

Drive the running instance with ui_driver.py, e.g.:
  ui_driver.py dump
  ui_driver.py find appLoader
  ui_driver.py click runAppButton
  ui_driver.py screenshot /tmp/app.png
"""

import argparse
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ui_driver import (  # noqa: E402
    REPO_ROOT,
    AutomationError,
    UiDriver,
    add_common_arguments,
    config_port,
    config_port_env,
    find_binary,
    load_config,
)

# State and log stay inside the repo (gitignored tmp/), so nothing the session
# writes lands outside the project directory.
SESSION_DIR = os.path.join(REPO_ROOT, "tmp", "ui_session")
STATE_PATH = os.path.join(SESSION_DIR, "session.json")
LOG_PATH = os.path.join(SESSION_DIR, "app.log")


def _load_state():
    try:
        with open(STATE_PATH, "r", encoding="utf-8") as handle:
            return json.load(handle)
    except (OSError, ValueError):
        return None


def _save_state(state):
    os.makedirs(SESSION_DIR, exist_ok=True)
    with open(STATE_PATH, "w", encoding="utf-8") as handle:
        json.dump(state, handle)


def _clear_state():
    try:
        os.remove(STATE_PATH)
    except OSError:
        pass


def _prepare_sandbox(config, run_dir=None):
    """Working directory and extra environment the app should be launched with.

    Without a "sandbox" section the app runs in the repo root with no extra
    environment, which is what an app that keeps no data in the repo wants.
    """
    sandbox = config.get("sandbox") or {}
    if not sandbox and not run_dir:
        return REPO_ROOT, {}

    directory = run_dir or sandbox.get("dir") or os.path.join("tmp", "run")
    directory = os.path.abspath(os.path.join(REPO_ROOT, directory))
    os.makedirs(directory, exist_ok=True)

    for name in sandbox.get("copy") or []:
        source = os.path.join(REPO_ROOT, name)
        target = os.path.join(directory, os.path.basename(name))
        if os.path.isfile(source) and not os.path.exists(target):
            shutil.copy2(source, target)

    env = {}
    for key, value in (sandbox.get("env") or {}).items():
        path = value if os.path.isabs(value) else os.path.join(directory, value)
        os.makedirs(path, exist_ok=True)
        env[key] = path

    return directory, env


def _pid_alive(pid):
    if not pid or pid < 0:
        return False
    try:
        os.kill(pid, 0)
    except OSError:
        return False
    return True


def _responds(port, timeout=1.0):
    driver = UiDriver(port=port)
    try:
        driver.connect(timeout=timeout)
        return True
    except AutomationError:
        return False
    finally:
        driver.close()


def _pid_on_port(port):
    """Best-effort: pid of whatever is listening on 127.0.0.1:port (Linux ss)."""
    try:
        out = subprocess.run(["ss", "-ltnp"], capture_output=True, text=True, timeout=3).stdout
    except (OSError, subprocess.SubprocessError):
        return None
    for line in out.splitlines():
        if re.search(rf"127\.0\.0\.1:{port}\b", line):
            match = re.search(r"pid=(\d+)", line)
            if match:
                return int(match.group(1))
    return None


def cmd_start(opts):
    if _responds(opts.port):
        pid = _pid_on_port(opts.port)
        _save_state({"pid": pid, "port": opts.port, "log": LOG_PATH})
        where = f"pid {pid}" if pid else "unknown pid"
        print(f"already running on 127.0.0.1:{opts.port} ({where}); using it")
        return 0

    binary = opts.binary or find_binary(opts.config)
    if not binary or not os.path.isfile(binary):
        print(
            "error: app binary not found; build it, pass --binary, or list it "
            f"under \"binary\" in {os.path.join(REPO_ROOT, '.ui_automation.json')}",
            file=sys.stderr,
        )
        return 1

    work_dir, sandbox_env = _prepare_sandbox(opts.config, opts.run_dir)

    child_env = os.environ.copy()
    child_env.update(sandbox_env)
    child_env[opts.port_env] = str(opts.port)
    if opts.platform:
        child_env["QT_QPA_PLATFORM"] = opts.platform

    os.makedirs(SESSION_DIR, exist_ok=True)
    log = open(LOG_PATH, "wb")
    proc = subprocess.Popen(
        [binary],
        cwd=work_dir,
        env=child_env,
        stdout=log,
        stderr=subprocess.STDOUT,
        stdin=subprocess.DEVNULL,
        start_new_session=True,
    )

    deadline = time.time() + opts.timeout
    while time.time() < deadline:
        if proc.poll() is not None:
            print(
                f"error: app exited early (code {proc.returncode}); see {LOG_PATH}",
                file=sys.stderr,
            )
            return 1
        if _responds(opts.port):
            _save_state({"pid": proc.pid, "port": opts.port, "log": LOG_PATH})
            print(
                f"started: pid {proc.pid} on 127.0.0.1:{opts.port} "
                f"(run dir: {work_dir}, log: {LOG_PATH})"
            )
            return 0
        time.sleep(0.25)

    print(
        f"error: app did not become ready within {opts.timeout:g}s; see {LOG_PATH}",
        file=sys.stderr,
    )
    proc.terminate()
    return 1


def cmd_status(opts):
    if _responds(opts.port):
        pid = (_load_state() or {}).get("pid") or _pid_on_port(opts.port)
        print(f"running: pid {pid} on 127.0.0.1:{opts.port}")
        return 0
    print("stopped")
    return 1


def cmd_stop(opts):
    pid = (_load_state() or {}).get("pid") or _pid_on_port(opts.port)
    if not _pid_alive(pid):
        _clear_state()
        print("not running")
        return 0
    for sig in (signal.SIGTERM, signal.SIGKILL):
        try:
            os.killpg(os.getpgid(pid), sig)
        except OSError:
            try:
                os.kill(pid, sig)
            except OSError:
                pass
        for _ in range(20):
            if not _pid_alive(pid):
                break
            time.sleep(0.25)
        if not _pid_alive(pid):
            break
    _clear_state()
    print(f"stopped: pid {pid}")
    return 0


def cmd_restart(opts):
    cmd_stop(opts)
    return cmd_start(opts)


def cmd_logs(opts):
    path = (_load_state() or {}).get("log", LOG_PATH)
    if not os.path.isfile(path):
        print(f"no log at {path}")
        return 1
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        lines = handle.readlines()
    for line in lines[-opts.lines :]:
        sys.stdout.write(line)
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Ensure a persistent app instance for UI automation."
    )
    sub = parser.add_subparsers(dest="command", required=True)

    for name in ("start", "restart"):
        p = sub.add_parser(name)
        add_common_arguments(p)
        p.add_argument("--timeout", type=float, default=20.0)
        p.add_argument(
            "--platform",
            default=None,
            help="Qt platform plugin for the launched app, e.g. offscreen",
        )
        p.add_argument(
            "--run-dir",
            default=None,
            help="working directory to sandbox the app in (default: .ui_automation.json)",
        )

    for name in ("status", "stop"):
        add_common_arguments(sub.add_parser(name))

    p = sub.add_parser("logs")
    p.add_argument("-n", "--lines", type=int, default=40)

    opts = parser.parse_args(argv)

    try:
        opts.config = load_config()
    except AutomationError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    if getattr(opts, "port", None) is None:
        opts.port = config_port(opts.config)
    opts.port_env = config_port_env(opts.config)

    handlers = {
        "start": cmd_start,
        "status": cmd_status,
        "stop": cmd_stop,
        "restart": cmd_restart,
        "logs": cmd_logs,
    }
    try:
        return handlers[opts.command](opts)
    except AutomationError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
