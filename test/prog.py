#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from subprocess import Popen, PIPE, TimeoutExpired
import platform
import os
import shutil

def pipe_through_prog(argv, text, timeout=None):
    p1 = Popen(argv, stdout=PIPE, stdin=PIPE, stderr=PIPE)
    try:
        [result, err] = p1.communicate(input=text.encode('utf-8'), timeout=timeout)
    except TimeoutExpired:
        p1.kill()
        p1.communicate()
        raise
    return [p1.returncode, result.decode('utf-8'), err]

class Prog:
    def __init__(self, cmdline="md2html", default_options=[]):
        program = shutil.which(cmdline)
        if not program and os.name == 'nt':
            program = shutil.which(cmdline + '.exe')
        self.cmdline = [cmdline] if program else cmdline.split()
        if len(self.cmdline) <= 1:
            # cmdline provided no command line options. Use default ones.
            if isinstance(default_options, str):
                self.cmdline += default_options.split()
            else:
                self.cmdline += default_options

    def to_html(self, text, timeout=None):
        return pipe_through_prog(self.cmdline, text, timeout)
