#!/usr/bin/env python3
"""Drive the running app towards a goal with a small Claude model, one cheap call per step.

Each step feeds the model only the goal, its last few actions and a one-line-per-control
view of the screen, through `claude -p` (no tools, no CLAUDE.md, no skills, no session
kept). The model answers with a single action line; this script runs it through
ui_driver.UiDriver and repeats. The caller sees one verdict line, the details go to
tmp/ui_agent/last.log.

    python3 libs/tools/ui_agent.py "open the first course" --max-steps 12
    python3 libs/tools/ui_agent.py "type 5 and submit" --expect resultLabel:text=5
"""

import argparse
import base64
import json
import os
import subprocess
import sys
import time

from ui_driver import (
    REPO_ROOT,
    AutomationError,
    UiDriver,
    config_port,
    config_port_env,
    load_config,
)

SESSION_SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ui_session.py")
WORK_DIR = os.path.join(REPO_ROOT, "tmp", "ui_agent")
LOG_PATH = os.path.join(WORK_DIR, "last.log")
HISTORY_SIZE = 6
MAX_SCREEN_LINES = 80
MAX_TEXT_LENGTH = 40
REPEAT_LIMIT = 3

SYSTEM_PROMPT = (
    "You operate a GUI under test. Input: GOAL, RECENT actions and SCREEN, one control "
    "per line as 'objectName Class props'. Answer with exactly one line, nothing else:\n"
    "click NAME\nset NAME TEXT\nkey SEQUENCE\nscroll NAME PIXELS\ndone\nfail REASON\n"
    "Use only names present in SCREEN. Answer done once the goal is reached, fail when "
    "it cannot be reached."
)


def describe(node):
    parts = [node["objectName"], node.get("class", "")]
    for key in ("text", "currentText", "value"):
        if key in node:
            text = str(node[key]).replace("\n", " ")
            parts.append(f'{key}="{text[:MAX_TEXT_LENGTH]}"')
    if "checked" in node:
        parts.append("checked" if node["checked"] else "unchecked")
    if node.get("enabled") is False:
        parts.append("disabled")
    return " ".join(parts)


def render_screen(tree):
    names = []
    lines = []

    def walk(nodes):
        for node in nodes:
            if node.get("visible") is False:
                continue
            if node.get("objectName"):
                names.append(node["objectName"])
                lines.append(describe(node))
            walk(node.get("children", []))

    walk(tree)
    return names, "\n".join(lines[:MAX_SCREEN_LINES])


def user_message_with_image(prompt, image_path):
    with open(image_path, "rb") as handle:
        data = base64.b64encode(handle.read()).decode("ascii")
    content = [
        {"type": "image", "source": {"type": "base64", "media_type": "image/png", "data": data}},
        {"type": "text", "text": prompt},
    ]
    return json.dumps({"type": "user", "message": {"role": "user", "content": content}})


def parse_result(stdout, stderr):
    for line in reversed(stdout.splitlines()):
        try:
            reply = json.loads(line)
        except ValueError:
            continue
        if reply.get("type") == "result":
            return reply
    raise AutomationError(f"claude returned no result: {stderr.strip()[:200]}")


def call_claude(model, system_prompt, prompt, image_path=None):
    os.makedirs(WORK_DIR, exist_ok=True)
    command = [
        "claude", "-p",
        "--model", model,
        "--safe-mode",
        "--tools", "",
        "--no-session-persistence",
        "--system-prompt", system_prompt,
    ]
    if image_path:
        command += ["--input-format", "stream-json", "--output-format", "stream-json", "--verbose"]
        stdin = user_message_with_image(prompt, image_path)
    else:
        command += ["--output-format", "json"]
        stdin = prompt
    proc = subprocess.run(
        command, input=stdin, capture_output=True, text=True, cwd=WORK_DIR, timeout=120
    )
    reply = parse_result(proc.stdout, proc.stderr)
    if reply.get("is_error"):
        raise AutomationError(f"claude error: {str(reply.get('result'))[:200]}")
    return reply["result"], reply.get("total_cost_usd", 0.0)


def ask_model(model, prompt):
    text, cost = call_claude(model, SYSTEM_PROMPT, prompt)
    lines = [line.strip() for line in text.splitlines() if line.strip()]
    return (lines[0] if lines else ""), cost


def execute(driver, names, action):
    verb, _, rest = action.partition(" ")
    target, _, argument = rest.partition(" ")
    if verb in ("click", "set", "scroll") and target not in names:
        raise AutomationError(f"no such control: {target}")
    if verb == "click":
        driver.click(target)
    elif verb == "set":
        driver.set_text(target, argument)
    elif verb == "scroll":
        driver.scroll(target, int(argument))
    elif verb == "key":
        driver.key(rest)
    else:
        raise AutomationError(f"unknown action: {action}")


def check_expectations(driver, expectations):
    for expectation in expectations:
        target, _, wanted = expectation.partition("=")
        name, _, prop = target.partition(":")
        actual = str(driver.get(name, prop))
        if actual != wanted:
            return f"expected {name}.{prop}={wanted!r}, got {actual!r}"
    return None


def ensure_session(opts):
    command = [sys.executable, SESSION_SCRIPT, "start", "--port", str(opts.port)]
    if opts.platform:
        command += ["--platform", opts.platform]
    proc = subprocess.run(command, capture_output=True, text=True)
    if proc.returncode != 0:
        raise AutomationError(proc.stderr.strip() or proc.stdout.strip())


def run(opts, driver, log):
    history = []
    total_cost = 0.0
    repeats = 0
    previous = None

    for step in range(1, opts.max_steps + 1):
        names, screen = render_screen(driver.dump())
        recent = "\n".join(history[-HISTORY_SIZE:]) or "(none)"
        prompt = f"GOAL: {opts.goal}\nRECENT:\n{recent}\nSCREEN:\n{screen}"
        action, cost = ask_model(opts.model, prompt)
        total_cost += cost

        repeats = repeats + 1 if action == previous else 1
        previous = action
        if repeats >= REPEAT_LIMIT:
            log.write(f"{step} {action} -> stuck\n")
            return "FAIL", f"stuck repeating '{action}'", step, total_cost

        if action == "done":
            log.write(f"{step} done\n")
            problem = check_expectations(driver, opts.expect)
            if problem:
                return "FAIL", problem, step, total_cost
            return "PASS", "", step, total_cost
        if action.startswith("fail"):
            log.write(f"{step} {action}\n")
            return "FAIL", action[4:].strip() or "model gave up", step, total_cost

        try:
            execute(driver, names, action)
            outcome = "ok"
        except (AutomationError, ValueError) as exc:
            outcome = f"ERR {exc}"
        history.append(f"{action} -> {outcome}")
        log.write(f"{step} {action} -> {outcome} (${cost:.5f})\n")
        if opts.verbose:
            print(f"{step}: {action} -> {outcome}")
        time.sleep(opts.settle)

    return "FAIL", "step limit reached", opts.max_steps, total_cost


def main(argv=None):
    parser = argparse.ArgumentParser(description="Let a small Claude model drive the app.")
    parser.add_argument("goal")
    parser.add_argument("--max-steps", type=int, default=15)
    parser.add_argument("--model", default="haiku")
    parser.add_argument("--expect", action="append", default=[], metavar="NAME:PROP=VALUE")
    parser.add_argument("--settle", type=float, default=0.3)
    parser.add_argument("--port", type=int)
    parser.add_argument("--platform")
    parser.add_argument("--no-start", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    opts = parser.parse_args(argv)

    config = load_config()
    if opts.port is None:
        opts.port = config_port(config)

    os.makedirs(WORK_DIR, exist_ok=True)
    driver = UiDriver(port=opts.port, port_env=config_port_env(config))
    try:
        if not opts.no_start:
            ensure_session(opts)
        driver.connect()
        with open(LOG_PATH, "w", encoding="utf-8") as log:
            log.write(f"GOAL: {opts.goal}\n")
            verdict, reason, steps, cost = run(opts, driver, log)
    except (AutomationError, subprocess.TimeoutExpired) as exc:
        print(f"FAIL {exc}")
        return 1
    finally:
        driver.close()

    suffix = f" {reason}" if reason else ""
    print(f"{verdict}{suffix} steps={steps} cost=${cost:.4f} log=tmp/ui_agent/last.log")
    return 0 if verdict == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
