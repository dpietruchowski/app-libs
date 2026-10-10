#!/usr/bin/env python3
"""Turn a screenshot of the running app into an ASCII drawing, using a small Claude model.

The screenshot goes to `claude -p` as an image (no tools, no CLAUDE.md, no skills, no
session kept) and the model answers with the drawing only. The grid keeps the window's
aspect ratio. Cost goes to stderr so stdout is just the drawing.

    python3 libs/tools/ui_ascii.py                      # screenshot the app, draw it
    python3 libs/tools/ui_ascii.py --columns 50 --out tmp/screen.txt
    python3 libs/tools/ui_ascii.py --image tmp/ui_agent/shot.png
"""

import argparse
import os
import struct
import sys

from ui_agent import WORK_DIR, call_claude
from ui_driver import AutomationError, UiDriver, config_port, config_port_env, load_config

SHOT_PATH = os.path.join(WORK_DIR, "shot.png")
CHARACTER_ASPECT = 0.5

SYSTEM_PROMPT = (
    "You convert a GUI screenshot into a plain-text ASCII drawing of exactly the requested "
    "grid. Answer with the drawing only: no explanation, no code fence. Draw each visible "
    "element where it sits, keeping proportions. Frame containers and cards with + - |. "
    "Write every visible text verbatim. Buttons as [ Label ], text fields as [____ text ], "
    "checkboxes as [x] or [ ], radio buttons as (o) or ( ), switches as (on ) or ( off), "
    "icons as a single {name} token, images as a box with an X inside. Skip decoration "
    "that carries no information."
)


def png_size(path):
    with open(path, "rb") as handle:
        header = handle.read(24)
    return struct.unpack(">II", header[16:24])


def grid_rows(image_path, columns):
    width, height = png_size(image_path)
    return max(1, round(columns * height / width * CHARACTER_ASPECT))


def screenshot_to_ascii(image_path, model="haiku", columns=60):
    rows = grid_rows(image_path, columns)
    prompt = f"Draw this screenshot as ASCII on a grid of {columns} columns by {rows} rows."
    return call_claude(model, SYSTEM_PROMPT, prompt, image_path=image_path)


def main(argv=None):
    parser = argparse.ArgumentParser(description="Draw a screenshot of the app as ASCII.")
    parser.add_argument("--image", help="convert this PNG instead of taking a screenshot")
    parser.add_argument("--columns", type=int, default=60)
    parser.add_argument("--model", default="haiku")
    parser.add_argument("--out", help="also write the drawing to this file")
    parser.add_argument("--port", type=int)
    opts = parser.parse_args(argv)

    try:
        image_path = opts.image
        if image_path is None:
            config = load_config()
            port = config_port(config) if opts.port is None else opts.port
            driver = UiDriver(port=port, port_env=config_port_env(config))
            driver.connect()
            try:
                os.makedirs(WORK_DIR, exist_ok=True)
                driver.screenshot(SHOT_PATH)
            finally:
                driver.close()
            image_path = SHOT_PATH
        drawing, cost = screenshot_to_ascii(image_path, opts.model, opts.columns)
    except AutomationError as exc:
        print(f"FAIL {exc}", file=sys.stderr)
        return 1

    print(drawing)
    print(f"list_cost=${cost:.4f}", file=sys.stderr)
    if opts.out:
        with open(opts.out, "w", encoding="utf-8") as handle:
            handle.write(drawing + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
