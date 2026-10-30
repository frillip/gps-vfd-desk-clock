#!/usr/bin/env python3
"""
Checks the include layering of the firmware: a folder includes only from
itself and the folders below it in LAYERS (and from ../common, below all of
them). main.c, at the top, may include anything.

Run from anywhere: python3 tools/layers.py. Exits 1 and lists each include
that reaches upwards; within a folder, each pair of files that include each
other is listed too.
"""
import os
import re
import sys

# Bottom first.
LAYERS = ["platform", "drivers", "console", "timing", "gnss", "ui", "sync", "settings", "menu", "esp"]

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)


def folder_of(path):
    relative = os.path.relpath(path, ROOT)
    if relative.startswith(".."):
        return "common"
    head = relative.split(os.sep)[0]
    return head if head in LAYERS else "main"


def rank(folder):
    if folder == "common":
        return -1
    if folder == "main":
        return len(LAYERS)
    return LAYERS.index(folder)


def sources():
    for directory, subdirectories, files in os.walk(ROOT):
        subdirectories[:] = [d for d in subdirectories if d not in ("tests", "nbproject", "build", "dist", "tools")]
        for name in sorted(files):
            if name.endswith((".c", ".h")):
                yield os.path.join(directory, name)


def main():
    problems = []
    edges = {}
    for path in sources():
        with open(path, encoding="utf-8", errors="replace") as handle:
            text = handle.read()
        source = folder_of(path)
        stem = os.path.splitext(os.path.relpath(path, ROOT))[0]
        for included in INCLUDE.findall(text):
            target_path = os.path.normpath(os.path.join(os.path.dirname(path), included))
            target = folder_of(target_path)
            target_stem = os.path.splitext(os.path.relpath(target_path, ROOT))[0]
            if target_stem != stem:
                edges.setdefault(stem, set()).add(target_stem)
            if rank(target) > rank(source):
                problems.append("%s: %s includes %s (%s above %s)"
                                % (os.path.relpath(path, ROOT), source, included, target, source))
    for stem, targets in sorted(edges.items()):
        for target in sorted(targets):
            if stem < target and stem in edges.get(target, ()):
                problems.append("%s and %s include each other" % (stem, target))
    for problem in problems:
        print(problem)
    print("layers: %s; %d problem%s" % (" < ".join(["common"] + LAYERS + ["main"]), len(problems),
                                         "" if len(problems) == 1 else "s"))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
