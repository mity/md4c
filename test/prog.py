#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from subprocess import Popen, PIPE
import platform
import os

def pipe_through_prog(prog, text):
    p1 = Popen(prog, stdout=PIPE, stdin=PIPE, stderr=PIPE)
    [result, err] = p1.communicate(input=text.encode('utf-8'))
    return [p1.returncode, result.decode('utf-8'), err]

class Prog:
    def __init__(self, prog="md2html", default_options=[]):
        self.prog = prog.split()
        if len(self.prog) <= 1:
            # prog provided no command line options. Use default ones.
            if isinstance(default_options, str):
                self.prog += default_options.split()
            else:
                self.prog += default_options
        self.to_html = lambda x: pipe_through_prog(self.prog, x)
