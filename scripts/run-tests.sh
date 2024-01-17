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

if which py >>/dev/null 2>&1; then
    PYTHON=py
elif which python3 >>/dev/null 2>&1; then
    PYTHON=python3
elif which python >>/dev/null 2>&1; then
    if [ `python --version | awk '{print $2}' | cut -d. -f1` -ge 3 ]; then
        PYTHON=python
    fi
fi

echo
echo "CommonMark specification:"
$PYTHON "$TEST_DIR/run-testsuite.py" -s "$TEST_DIR/spec.txt" -p "$PROGRAM"

echo
echo "Permissive autolink extensions:"
$PYTHON "$TEST_DIR/run-testsuite.py" -s "$TEST_DIR/spec-permissive-autolinks.txt" -p "$PROGRAM"

echo
echo "Hard soft breaks extension:"
$PYTHON "$TEST_DIR/run-testsuite.py" -s "$TEST_DIR/spec-hard-soft-breaks.txt" -p "$PROGRAM"

echo
echo "Tables extension:"
$PYTHON "$TEST_DIR/run-testsuite.py" -s "$TEST_DIR/spec-tables.txt" -p "$PROGRAM"

echo
echo "Strikethrough extension:"
$PYTHON "$TEST_DIR/run-testsuite.py" -s "$TEST_DIR/spec-strikethrough.txt" -p "$PROGRAM"

echo
echo "Task lists extension:"
$PYTHON "$TEST_DIR/run-testsuite.py" -s "$TEST_DIR/spec-tasklists.txt" -p "$PROGRAM"

echo
echo "LaTeX extension:"
$PYTHON "$TEST_DIR/run-testsuite.py" -s "$TEST_DIR/spec-latex-math.txt" -p "$PROGRAM"

echo
echo "Wiki links extension:"
$PYTHON "$TEST_DIR/run-testsuite.py" -s "$TEST_DIR/spec-wiki-links.txt" -p "$PROGRAM"

echo
echo "Underline extension:"
$PYTHON "$TEST_DIR/run-testsuite.py" -s "$TEST_DIR/spec-underline.txt" -p "$PROGRAM"

echo
echo "Code coverage:"
$PYTHON "$TEST_DIR/run-testsuite.py" -s "$TEST_DIR/coverage.txt" -p "$PROGRAM"

echo
echo "Regressions:"
$PYTHON "$TEST_DIR/run-testsuite.py" -s "$TEST_DIR/regressions.txt" -p "$PROGRAM"

echo
echo "Pathological inputs:"
$PYTHON "$TEST_DIR/pathological-tests.py" -p "$PROGRAM"
