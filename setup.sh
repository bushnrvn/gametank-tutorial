#!/bin/sh
# Fetch the GameTank SDK into ./sdk, at the version this tutorial was written against.
# (The SDK is not part of this repository: it is Clyde Shaffer's, and you get it from him.)
#
# You also need these installed:
#   cc65 (a recent snapshot, with the 65C02 target: https://cc65.github.io)   make   git   node   python3   zopfli
#   macOS: brew install cc65 node zopfli       Debian/Ubuntu: apt install cc65 nodejs zopfli make git python3
set -e
cd "$(dirname "$0")"
SDK_REPO=https://github.com/clydeshaffer/gametank_sdk
SDK_COMMIT=18b281f6ca31fe6d83260f8c974b1a2dd77435b2
for tool in cl65 make git node python3 zopfli; do
    command -v $tool >/dev/null || { echo "missing: $tool (see the notes at the top of setup.sh)"; exit 1; }
done
if [ ! -d sdk/.git ]; then git clone "$SDK_REPO" sdk; fi
git -C sdk fetch -q origin
git -C sdk checkout -q "$SDK_COMMIT"
echo "SDK ready in ./sdk (commit $SDK_COMMIT)"
echo "Try:  ./build.sh lessons/01-the-screen   and load bin/01-the-screen.gtr in the GameTank emulator"
