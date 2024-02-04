#!/usr/bin/env python3

import glob
import os
import subprocess
import sys


argv0_dir = os.path.dirname(sys.argv[0])
project_dir = os.path.abspath(os.path.join(argv0_dir, ".."))
test_dir = os.path.join(project_dir, "test")
program = os.path.abspath(os.path.join("md2html", "md2html"))

if __name__ == "__main__":
    err_count = 0

    os.chdir(test_dir)

    for testsuite in glob.glob('*.txt'):
        print("Testing {}".format(testsuite))
        sys.stdout.flush()
        sys.stderr.flush()
        args = [
                sys.executable,
                "run-testsuite.py",
                "-s", testsuite,
                "-p", str(program)
        ]
        p = subprocess.run(args)
        if p.returncode != 0:
            err_count += 1
        print()

    print("Testing pathological inputs:")
    sys.stdout.flush()
    sys.stderr.flush()
    args = [
            sys.executable,
            "pathological-tests.py",
            "-p", str(program)
    ]
    subprocess.run(args)
    if p.returncode != 0:
        err_count += 1

    sys.exit(err_count)
