#!/usr/bin/env bash
set -eo pipefail
cd "$(dirname "$0")/.."
PROG=${0##*/}

function usage {
  echo "Usage: $PROG [options]"
  echo "options:"
  echo "  -debug       Build in debug mode instead of release mode"
  echo "  -lib=shared  Build dynamic shared library"
  echo "  -lib=static  Build static library"
  echo "  -clean       Recreate build directory and build from scratch"
  echo "  -w, -watch   Watch source files for changes and rebuild"
  echo "  -h, -help    Show help on stdout and exit"
  echo "Note on -lib:"
  echo "  If -lib is not provided, static library is built on Windows and"
  echo "  dynamic library is built on other OSes."
  echo ""
  echo "See https://github.com/mity/md4c/wiki/Building-MD4C for details"
}

OPT_LIB=
OPT_CLEAN=false
OPT_WATCH=false
BUILD_DIR=build/release
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
    BUILD_DIR=build/debug
    shift
    ;;
  -clean|--clean)
    OPT_CLEAN=true
    shift
    ;;
  -w|-watch|--watch)
    OPT_WATCH=true
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

if $OPT_CLEAN; then
  rm -rf "$BUILD_DIR"
fi

SRCDIR=$PWD
BUILD_CMD=make

if [ -d "$BUILD_DIR" ]; then
  cd "$BUILD_DIR"
  if [ -f build.ninja ]; then
    BUILD_CMD=ninja
  fi
else
  CC=$CC
  if [ -z $CC ] && (which clang >/dev/null); then
    export CC=clang
  fi
  if [ "$CC" == "clang" ] || [[ "$CC" == *"/clang" ]]; then
    # enable colorized output (when stdout/stderr is a TTY) and warn about
    # uninitialized variables that are initialized only in some conditions.
    export CFLAGS="$CFLAGS -fcolor-diagnostics -Wconditional-uninitialized"
  fi

  mkdir -p "$BUILD_DIR"
  cd "$BUILD_DIR"

  if (which ninja >/dev/null); then
    cmake -G Ninja "$SRCDIR"
    BUILD_CMD=ninja
  elif (which make >/dev/null); then
    cmake -G "Unix Makefiles" "$SRCDIR"
  else
    echo "Could not find ninja nor make in PATH" >&2
    exit 1
  fi
fi

$BUILD_CMD

if $OPT_WATCH; then
  if ! (which fswatch >/dev/null); then
    echo "fswatch not found in PATH (needed with -watch option)" >&2
    echo "See http://emcrisostomo.github.io/fswatch/" >&2
    if (which brew >/dev/null); then
      echo "  brew info fswatch"
    fi
    if (which apt >/dev/null); then
      echo "  apt install fswatch"
    fi
    exit 1
  fi
  trap 'echo -en "\r"' EXIT  # reset line on ^C
  trap exit SIGINT  # make sure we can ctrl-c in the while loop
  while true; do
    echo "$PROG: Watching source files for changes..."
    pushd "$SRCDIR" >/dev/null
    fswatch --one-event --latency=0.2 src/*.c src/*.h
    popd >/dev/null
    echo -e "\x1bc"
    $BUILD_CMD
  done
fi
