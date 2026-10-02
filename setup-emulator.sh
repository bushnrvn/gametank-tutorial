#!/bin/sh
# Build the GameTank emulator into ./GameTankEmulator, with the small scripting patch lesson 11 uses to test games automatically.
# You do NOT need this to play the lessons: any GameTank emulator (or the real console) runs bin/*.gtr. It is only for tools/drive.py.
#
# Needs: git, make, a C++ compiler, SDL2 (macOS: brew install sdl2; Debian/Ubuntu: apt install libsdl2-dev libx11-dev)
set -e
cd "$(dirname "$0")"
EMU_REPO=https://github.com/clydeshaffer/GameTankEmulator
EMU_COMMIT=9896544
[ -d GameTankEmulator/.git ] || git clone --recursive "$EMU_REPO" GameTankEmulator
cd GameTankEmulator
git checkout -q -f "$EMU_COMMIT"
git submodule update --init --recursive
patch -p1 < ../tools/emulator-script.patch
make install
echo "built: $PWD/bin/GameTankEmulator   (tools/drive.py finds it there)"
