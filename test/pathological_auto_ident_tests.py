#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import re
import argparse
import sys
import platform
from cmark import CMark
from timeit import default_timer as timer

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Run cmark tests.')
    parser.add_argument('-p', '--program', dest='program', nargs='?', default=None,
            help='program to test')
    parser.add_argument('--library-dir', dest='library_dir', nargs='?',
            default=None, help='directory containing dynamic library')
    args = parser.parse_args(sys.argv[1:])

cmark = CMark(prog=args.program, library_dir=args.library_dir)

# list of pairs consisting of input and a regex that must match the output.
pathological = {
    # note - some pythons have limit of 65535 for {num-matches} in re.

    "many identical heading":
            (("# a\n" * (50000+1)),
            re.compile("^<h1 id=\"a\">a</h1>\n(<h1 id=\"a-\d+\">a</h1>\n){50000}$"))
}

whitespace_re = re.compile('/s+/')
passed = 0
errored = 0
failed = 0

#print("Testing pathological cases:")
for description in pathological:
    (inp, regex) = pathological[description]
    start = timer()
    [rc, actual, err] = cmark.to_html(inp)
    end = timer()
    if rc != 0:
        errored += 1
        print('{:35} [ERRORED (return code %d)]'.format(description, rc))
        print(err)
    elif regex.search(actual):
        print('{:35} [PASSED] {:.3f} secs'.format(description, end-start))
        passed += 1
    else:
        print('{:35} [FAILED]'.format(description))
        print(repr(actual))
        failed += 1

print("%d passed, %d failed, %d errored" % (passed, failed, errored))
if (failed == 0 and errored == 0):
    exit(0)
else:
    exit(1)
