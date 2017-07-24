#!/bin/sh
#
# Run this script from build directory.

#set -e

SELF_DIR=`dirname $0`
PROJECT_DIR="$SELF_DIR/.."
TEST_DIR="$PROJECT_DIR/test"


PROGRAM="md2html/md2html"
if [ ! -x "$PROGRAM" ]; then
    echo "Cannot find the $PROGRAM." >&2
    echo "You have to run this script from the build directory." >&2
    exit 1
fi

if which python3 2>/dev/null; then
    PYTHON=python3
elif which python 2>/dev/null; then
    if [ `python --version | awk '{print $2}' | cut -d. -f1` -ge 3 ]; then
        PYTHON=python
    fi
fi

# Test CommonMark specification compliance
# (using the vanilla specification file):
$PYTHON "$TEST_DIR/spec_tests.py" -s "$TEST_DIR/spec.txt" -p "$PROGRAM"

# More tests for better coverage ten what the spec provides:
$PYTHON "$TEST_DIR/spec_tests.py" -s "$TEST_DIR/coverage.txt" -p "$PROGRAM"

# Test various extensions and deviations from the specifications:
$PYTHON "$TEST_DIR/spec_tests.py" -s "$TEST_DIR/permissive-email-autolinks.txt" -p "$PROGRAM --fpermissive-email-autolinks"
$PYTHON "$TEST_DIR/spec_tests.py" -s "$TEST_DIR/permissive-url-autolinks.txt" -p "$PROGRAM --fpermissive-url-autolinks"
$PYTHON "$TEST_DIR/spec_tests.py" -s "$TEST_DIR/permissive-www-autolinks.txt" -p "$PROGRAM --fpermissive-www-autolinks"
$PYTHON "$TEST_DIR/spec_tests.py" -s "$TEST_DIR/tables.txt" -p "$PROGRAM --ftables"
$PYTHON "$TEST_DIR/spec_tests.py" -s "$TEST_DIR/strikethrough.txt" -p "$PROGRAM --fstrikethrough"

# Run pathological tests:
$PYTHON "$TEST_DIR/pathological_tests.py" -p "$PROGRAM"
