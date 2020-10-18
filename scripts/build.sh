#!/usr/bin/env bash
set -eo pipefail
cd "$(dirname "$0")/.."

if ! (which cmake >/dev/null); then
  echo "cmake not found in PATH" >&2
  exit 1
fi

rm -rf build
mkdir build
cd build

if (which ninja >/dev/null); then
  cmake -G Ninja ..
  ninja
elif (which make >/dev/null); then
  cmake -G "Unix Makefiles" ..
  make
else
  echo "Could not find ninja nor make in PATH" >&2
  exit 1
fi
