#!/usr/bin/env python3
"""Drive any Qt/QML app that links UiAutomationServer, over its automation port.

The app must be built with the automation server compiled in (see
libs/cpp/automation/uiautomationserver.h) and started with its port env var set.
Which env var and which binary differ per app, so this script reads them from a
`.ui_automation.json` at the consuming repo's root:

    {
      "binary": ["build-desktop/src/appfoo", "build/src/appfoo"],
      "port": 49200,
      "port_env": "FOO_AUTOMATION_PORT"
    }

`binary` may be one path or a list tried in order; every path is relative to the
repo root. Without the file the defaults are port 49200, env var
UI_AUTOMATION_PORT, and no binary (so `launch`/`shell` need --binary). Anything
in the file can be overridden by a CLI flag.

Why a server and not a standalone PySide script: the app is a compiled C++ Qt
binary. Its QObject/QML tree lives in that process's memory, so QObject.findChild
from a separate Python process cannot reach it. The app exposes a tiny localhost
TCP server that walks the tree and dispatches actions by objectName; this script
is just its client. Only objects with an objectName are reachable, so name every
item you want to drive or assert on.

Examples:
    ./ui_driver.py shell                 # launch the configured binary, then a REPL
    ./ui_driver.py dump                  # against an already-running instance
    ./ui_driver.py find appLoader
    ./ui_driver.py click runAppButton
    ./ui_driver.py get appLoader status
    ./ui_driver.py screenshot /tmp/app.png

    # control the app clock (needs libs/cpp/async TimeProvider)
    ./ui_driver.py set_time 2026-01-05
    ./ui_driver.py advance_time 3
    ./ui_driver.py reset_time
"""

import argparse
import json
import os
import shlex
import socket
import subprocess
import sys
import time

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 49200
DEFAULT_PORT_ENV = "UI_AUTOMATION_PORT"
CONFIG_NAME = ".ui_automation.json"

# This file lives in <repo>/libs/tools/, so the consuming repo is two up.
REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
)


class AutomationError(RuntimeError):
    pass


# --- per-repo configuration ---------------------------------------------------


def load_config(repo_root=REPO_ROOT):
    """Read `.ui_automation.json` from the repo root; missing file is fine."""
    path = os.path.join(repo_root, CONFIG_NAME)
    try:
        with open(path, "r", encoding="utf-8") as handle:
            config = json.load(handle)
    except OSError:
        return {}
    except ValueError as exc:
        raise AutomationError(f"{path}: {exc}") from exc
    if not isinstance(config, dict):
        raise AutomationError(f"{path}: expected a JSON object")
    return config


def config_port(config):
    return int(config.get("port", DEFAULT_PORT))


def config_port_env(config):
    return config.get("port_env", DEFAULT_PORT_ENV)


def find_binary(config, repo_root=REPO_ROOT):
    """First configured binary path that exists, as an absolute path."""
    candidates = config.get("binary") or []
    if isinstance(candidates, str):
        candidates = [candidates]
    for candidate in candidates:
        path = candidate if os.path.isabs(candidate) else os.path.join(repo_root, candidate)
        if os.path.isfile(path):
            return path
    return None


class UiDriver:
    def __init__(self, host=DEFAULT_HOST, port=DEFAULT_PORT, port_env=DEFAULT_PORT_ENV,
                 repo_root=REPO_ROOT):
        self.host = host
        self.port = port
        self.port_env = port_env
        self.repo_root = repo_root
        self._sock = None
        self._stream = None
        self._proc = None

    @classmethod
    def from_config(cls, host=DEFAULT_HOST, port=None, repo_root=REPO_ROOT):
        config = load_config(repo_root)
        return cls(
            host=host,
            port=config_port(config) if port is None else port,
            port_env=config_port_env(config),
            repo_root=repo_root,
        )

    # --- connection lifecycle -------------------------------------------------

    def launch(self, binary, env=None, timeout=20.0):
        """Spawn the binary with automation enabled and connect to it.

        The child runs in the repo root so any file it opens relative to the
        working directory lands there, whatever directory the caller is in.
        """
        child_env = os.environ.copy()
        if env:
            child_env.update(env)
        child_env[self.port_env] = str(self.port)
        self._proc = subprocess.Popen([binary], cwd=self.repo_root, env=child_env)
        self.connect(timeout=timeout)
        return self

    def connect(self, timeout=10.0):
        """Connect to a running instance, waiting for the server to come up."""
        deadline = time.time() + timeout
        last_err = None
        while time.time() < deadline:
            if self._proc is not None and self._proc.poll() is not None:
                raise AutomationError(f"app exited early with code {self._proc.returncode}")
            try:
                self._sock = socket.create_connection((self.host, self.port), timeout=2.0)
                self._stream = self._sock.makefile("rwb")
                self.ping()
                return self
            except (OSError, AutomationError) as exc:
                last_err = exc
                self._close_socket()
                time.sleep(0.25)
        raise AutomationError(f"could not connect to {self.host}:{self.port}: {last_err}")

    def close(self):
        self._close_socket()

    def terminate(self):
        """Close the connection and stop the app we launched (if any)."""
        self._close_socket()
        if self._proc is not None and self._proc.poll() is None:
            self._proc.terminate()
            try:
                self._proc.wait(timeout=5.0)
            except subprocess.TimeoutExpired:
                self._proc.kill()
        self._proc = None

    def _close_socket(self):
        if self._stream is not None:
            try:
                self._stream.close()
            except OSError:
                pass
            self._stream = None
        if self._sock is not None:
            try:
                self._sock.close()
            except OSError:
                pass
            self._sock = None

    # --- low level rpc --------------------------------------------------------

    def rpc(self, **request):
        """Send one command and return its response; raises on an error reply.

        Public so an app can drive commands this client has no wrapper for
        (a server built with app-specific commands compiled in, say).
        """
        if self._stream is None:
            raise AutomationError("not connected")
        self._stream.write((json.dumps(request) + "\n").encode("utf-8"))
        self._stream.flush()
        reply = self._stream.readline()
        if not reply:
            raise AutomationError("connection closed by app")
        response = json.loads(reply.decode("utf-8"))
        if not response.get("ok"):
            raise AutomationError(response.get("error", "unknown error"))
        return response

    # --- commands -------------------------------------------------------------

    def ping(self):
        return self.rpc(cmd="ping").get("pong", False)

    def dump(self):
        """Return the nested tree of named (objectName) objects."""
        return self.rpc(cmd="dump")["tree"]

    def find(self, substring=""):
        """Flat list of (objectName, class) whose name contains `substring`."""
        out = []

        def walk(nodes):
            for node in nodes:
                name = node.get("objectName", "")
                if substring in name:
                    out.append((name, node.get("class", "")))
                walk(node.get("children", []))

        walk(self.dump())
        return out

    def click(self, name):
        return self.rpc(cmd="click", name=name)

    def get(self, name, prop):
        return self.rpc(cmd="get", name=name, property=prop)["value"]

    # Properties that Qt Quick Controls only persist via interaction signals
    # (onToggled, onActivated, ...). Writing them with `set` changes the
    # property silently but the app never observes it, so the value reverts
    # on the next page load. Use `click` instead.
    _NOT_SETTABLE_PROPERTIES = {"checked", "currentIndex", "currentText"}

    def set(self, name, prop, value):
        if prop in self._NOT_SETTABLE_PROPERTIES:
            raise AutomationError(
                f"set {name} {prop} ... does not work: Qt Quick Controls only "
                f"apply '{prop}' changes via user interaction (onToggled/onActivated). "
                f"Use 'click' on the control instead."
            )
        return self.rpc(cmd="set", name=name, property=prop, value=value)

    def set_text(self, name, text):
        return self.set(name, "text", text)

    def invoke(self, name, method, *args):
        return self.rpc(cmd="invoke", name=name, method=method, args=list(args))

    def tap(self, x, y):
        return self.rpc(cmd="tap", x=x, y=y)

    def key(self, name, text=""):
        """Send a key press/release to the window, e.g. Tab, Down, Return, Shift+Tab.

        The name is a portable key sequence (QKeySequence). Pass `text` for
        printable keys that should also insert a character.
        """
        return self.rpc(cmd="key", name=name, text=text)

    def scrollto(self, name):
        """Scroll the nearest scrollable ancestor until the item is in view.

        `click` does this by itself, so you only need `scrollto` to read an
        off-screen item's on-screen position. Returns the item's scene {x, y}.
        """
        return self.rpc(cmd="scrollto", name=name)

    def scroll(self, name, dy, dx=0):
        """Scroll a Flickable/ListView (found by name or its first flickable child).

        `dy` is in pixels: positive scrolls down, negative scrolls up; `dx`
        scrolls horizontally. Returns {contentY, contentX, atBeginning, atEnd}.
        """
        return self.rpc(cmd="scroll", name=name, dy=dy, dx=dx)

    def screenshot(self, path):
        return self.rpc(cmd="screenshot", path=os.path.abspath(path))

    def get_time(self):
        """Return {dateTime, date, mocked} for the app's current clock."""
        return self.rpc(cmd="get_time")

    def set_time(self, value):
        """Pin the clock to an absolute date or datetime (ISO 8601).

        A bare `YYYY-MM-DD` sets the date and keeps the time of day; a full
        `YYYY-MM-DDThh:mm:ss` sets both. Installs a mock clock on first use.
        """
        key = "dateTime" if "T" in value else "date"
        return self.rpc(cmd="set_time", **{key: value})

    def advance_time(self, days):
        """Move the (mock) clock forward by `days` days."""
        return self.rpc(cmd="advance_time", days=days)

    def reset_time(self):
        """Restore the real system clock."""
        return self.rpc(cmd="reset_time")


# --- pretty printing & CLI ----------------------------------------------------


def print_tree(nodes, indent=0):
    for node in nodes:
        name = node.get("objectName", "")
        cls = node.get("class", "")
        extra = []
        if "text" in node:
            extra.append(f'text={node["text"]!r}')
        if "currentText" in node:
            extra.append(f'currentText={node["currentText"]!r}')
        if "values" in node:
            extra.append("values=[" + ", ".join(repr(v) for v in node["values"]) + "]")
        if "value" in node:
            extra.append(f'value={node["value"]}')
        if "checked" in node:
            extra.append("checked" if node["checked"] else "unchecked")
        if node.get("visible") is False:
            extra.append("hidden")
        if node.get("enabled") is False:
            extra.append("disabled")
        suffix = ("  [" + ", ".join(extra) + "]") if extra else ""
        print("  " * indent + f"{name} ({cls}){suffix}")
        print_tree(node.get("children", []), indent + 1)


def coerce(value):
    """Best-effort scalar coercion for CLI args (bool/int/float/str)."""
    lowered = value.lower()
    if lowered in ("true", "false"):
        return lowered == "true"
    for cast in (int, float):
        try:
            return cast(value)
        except ValueError:
            pass
    return value


def run_shell(driver):
    print("Connected. Commands: dump | find <s> | click <name> | get <name> <prop> |")
    print("  set <name> <prop> <value> | invoke <name> <method> [args...] |")
    print("  tap <x> <y> | key <sequence> [text] | scroll <name> <dy> [dx] |")
    print("  scrollto <name> | screenshot <path> |")
    print("  get_time | set_time <iso> | advance_time <days> | reset_time | quit")
    while True:
        try:
            raw = input("ui> ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            break
        if not raw:
            continue
        parts = shlex.split(raw)
        cmd, args = parts[0], parts[1:]
        if cmd in ("quit", "exit"):
            break
        try:
            dispatch(driver, cmd, args)
        except AutomationError as exc:
            print(f"error: {exc}")
        except (TypeError, IndexError):
            print(f"bad arguments for {cmd}")


def dispatch(driver, cmd, args):
    if cmd == "dump":
        print_tree(driver.dump())
    elif cmd == "find":
        for name, cls in driver.find(args[0] if args else ""):
            print(f"{name} ({cls})")
    elif cmd == "click":
        driver.click(args[0])
        print("ok")
    elif cmd == "get":
        print(driver.get(args[0], args[1]))
    elif cmd == "set":
        driver.set(args[0], args[1], coerce(args[2]))
        print("ok")
    elif cmd == "invoke":
        driver.invoke(args[0], args[1], *[coerce(a) for a in args[2:]])
        print("ok")
    elif cmd == "tap":
        driver.tap(coerce(args[0]), coerce(args[1]))
        print("ok")
    elif cmd == "key":
        driver.key(args[0], args[1] if len(args) > 1 else "")
        print("ok")
    elif cmd == "scrollto":
        result = driver.scrollto(args[0])
        print(f"x={result['x']:.0f} y={result['y']:.0f}")
    elif cmd == "scroll":
        result = driver.scroll(args[0], coerce(args[1]), coerce(args[2]) if len(args) > 2 else 0)
        edge = " (top)" if result["atBeginning"] else (" (bottom)" if result["atEnd"] else "")
        print(f"contentY={result['contentY']:.0f}{edge}")
    elif cmd == "screenshot":
        driver.screenshot(args[0])
        print(f"saved {args[0]}")
    elif cmd == "get_time":
        info = driver.get_time()
        suffix = "" if info.get("mocked") else " (system clock)"
        print(f"{info['dateTime']}{suffix}")
    elif cmd == "set_time":
        print(driver.set_time(args[0])["dateTime"])
    elif cmd == "advance_time":
        print(driver.advance_time(coerce(args[0]))["dateTime"])
    elif cmd == "reset_time":
        print(driver.reset_time()["dateTime"])
    elif cmd == "ping":
        print("pong" if driver.ping() else "no")
    else:
        print(f"unknown command: {cmd}")


def add_common_arguments(parser):
    """Shared by this CLI and ui_session.py, so both configure the same way."""
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, help="default: .ui_automation.json, else 49200")
    parser.add_argument("--binary", help="path to the app binary (default: .ui_automation.json)")


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Drive a Qt/QML app with the UI automation server by objectName."
    )
    add_common_arguments(parser)
    parser.add_argument("command", nargs="?", default="shell")
    parser.add_argument("args", nargs="*")
    opts = parser.parse_args(argv)

    try:
        config = load_config()
    except AutomationError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    driver = UiDriver(
        host=opts.host,
        port=config_port(config) if opts.port is None else opts.port,
        port_env=config_port_env(config),
    )
    launched = False

    binary = opts.binary or (find_binary(config) if opts.command in ("launch", "shell") else None)

    try:
        if opts.command in ("launch", "shell") and binary:
            print(f"launching {binary} ({driver.port_env}={driver.port}) ...")
            driver.launch(binary)
            launched = True
        else:
            driver.connect()

        if opts.command == "launch":
            print("app running with automation enabled. Ctrl-C to stop.")
            try:
                while driver._proc is None or driver._proc.poll() is None:
                    time.sleep(0.5)
            except KeyboardInterrupt:
                pass
        elif opts.command == "shell":
            run_shell(driver)
        else:
            dispatch(driver, opts.command, opts.args)
    except AutomationError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    finally:
        if launched:
            driver.terminate()
        else:
            driver.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
