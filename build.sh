#!/bin/sh
# Build one lesson into bin/<lesson>.gtr
#   ./build.sh lessons/01-the-screen
# The SDK is not copied into this repo: ./setup.sh clones it into ./sdk (or set SDK_DIR to a checkout you already have).
# Each lesson only holds the files that differ from a fresh SDK project (src/main.c, assets/, project.json), so the
# build copies the SDK to build/<lesson>/, lays the lesson's files over it, and runs the SDK's own makefile there.
set -e
cd "$(dirname "$0")"
LESSON="${1%/}"
[ -d "$LESSON/code" ] || { echo "usage: ./build.sh lessons/<lesson>   (needs $LESSON/code)"; exit 1; }
NAME=$(basename "$LESSON")
SDK="${SDK_DIR:-$PWD/sdk}"
[ -f "$SDK/makefile" ] || { echo "no SDK at $SDK: run ./setup.sh first (or set SDK_DIR)"; exit 1; }
WORK="$PWD/build/$NAME"
rm -rf "$WORK"; mkdir -p "$WORK" bin
# the SDK without its own build output and tools (the tools are big, so they are linked, not copied)
(cd "$SDK" && tar -c --exclude=.git --exclude=./build --exclude=./bin --exclude=./tools --exclude=./src/gen .) | tar -x -C "$WORK"
ln -s "$SDK/tools" "$WORK/tools"
cp -R "$LESSON/code/." "$WORK/"
# a lesson that needs a change to the SDK's engine lists it in "sdk-patches" (see tools/sdk_patch.py)
[ -f "$LESSON/sdk-patches" ] && python3 "$PWD/tools/sdk_patch.py" "$WORK" $(cat "$LESSON/sdk-patches")
cd "$WORK"
make import >/dev/null
make 2>&1 | grep -E -i "error|warning: .*overflow|Segment" || true
ROM=$(ls bin/*.gtr | head -1)
cp "$ROM" "$OLDPWD/bin/$NAME.gtr"
echo "built bin/$NAME.gtr ($(wc -c < "$ROM" | tr -d ' ') bytes)"
