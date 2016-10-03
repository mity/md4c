#!/bin/sh

set -e

if [ ! -d commonmark ]; then
    git clone https://github.com/jgm/commonmark.git
fi

commonmark/test/spec_tests.py -s commonmark/spec.txt -p md2html/md2html

