#!/bin/sh

set -e

PROGRAM="md2html/md2html"
if [ ! -x "$PROGRAM" ]; then
    echo "Cannot find the $PROGRAM." >&2
    echo "You have to run this script from the build directory." >&2
    exit 1
fi

if [ ! -d commonmark ]; then
    git clone https://github.com/jgm/commonmark.git
fi

if which python3 2>/dev/null; then
    PYTHON=python3
elif which python 2>/dev/null; then
    if [ `python --version | awk '{print $2}' | cut -d. -f1` -ge 3 ]; then
        PYTHON=python
    fi
fi

$PYTHON commonmark/test/spec_tests.py -s commonmark/spec.txt -p "$PROGRAM" "$@"
