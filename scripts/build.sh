#!/usr/bin/env bash
set -eo pipefail
cd "$(dirname "$0")/.."
PROG=$0

function usage {
  echo "Usage: $PROG [options]"
  echo "options:"
  echo "  -debug       Build in debug mode instead of release mode"
  echo "  -lib=shared  Build dynamic shared library"
  echo "  -lib=static  Build static library"
  echo "  -h, -help    Show help on stdout and exit"
  echo "Note on -lib:"
  echo "  If -lib is not provided, static library is built on Windows and"
  echo "  dynamic library is built on other OSes."
  echo ""
  echo "See https://github.com/mity/md4c/wiki/Building-MD4C for details"
}

OPT_LIB=
CMAKE_ARGS=()
export CMAKE_BUILD_TYPE=release
export CFLAGS="-Wextra"

while [[ $# -gt 0 ]]; do
  case "$1" in
  -h|-help|--help)
    usage
    exit 0
    shift
    ;;
  -debug|--debug)
    export CMAKE_BUILD_TYPE=debug
    shift
    ;;
  -lib=shared|--lib=shared)
    CMAKE_ARGS+=( -DBUILD_SHARED_LIBS=ON )
    shift
    ;;
  -lib=static|--lib=static)
    CMAKE_ARGS+=( -DBUILD_SHARED_LIBS=OFF )
    shift
    ;;
  -*)
    echo "$PROG: Unknown command or option $1" >&2
    usage >&2
    exit 1
    shift
    ;;
  esac
done

if ! (which cmake >/dev/null); then
  echo "cmake not found in PATH" >&2
  exit 1
fi

CC=$CC
if [ -z $CC ] && (which clang >/dev/null); then
  export CC=clang
fi
if [ "$CC" == "clang" ] || [[ "$CC" == *"/clang" ]]; then
  # enable colorized output (when stdout/stderr is a TTY) and warn about
  # uninitialized variables that are initialized only in some conditions.
  export CFLAGS="$CFLAGS -fcolor-diagnostics -Wconditional-uninitialized"
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
