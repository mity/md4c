#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from subprocess import *
import platform
import os

def pipe_through_prog(argv, text):
    p1 = Popen(argv, stdout=PIPE, stdin=PIPE, stderr=PIPE)
    [result, err] = p1.communicate(input=text.encode('utf-8'))
    return [p1.returncode, result.decode('utf-8'), err]

class Prog:
    def __init__(self, cmdline="md2html", default_options=[]):
        self.cmdline = cmdline.split()
        if len(self.cmdline) <= 1:
            # cmdline provided no command line options. Use default ones.
            if isinstance(default_options, str):
                self.cmdline += default_options.split()
            else:
                self.cmdline += default_options
        self.to_html = lambda x: pipe_through_prog(self.cmdline, x)
