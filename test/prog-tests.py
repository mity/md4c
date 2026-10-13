#!/usr/bin/env python3

import glob
import os
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch
from prog import Prog


class ProgramTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix='md4c prog ')
        self.addCleanup(directory.cleanup)
        self.directory = directory.name
        suffix = '.exe' if os.name == 'nt' else ''
        self.program = os.path.join(self.directory, 'md2html' + suffix)
        if os.name == 'nt':
            shutil.copy2(program, self.program)
            directories = [os.path.dirname(program),
                           os.path.join(os.path.dirname(program), '..', 'src')]
            for directory in directories:
                for library in glob.glob(os.path.join(directory, '*.dll')):
                    shutil.copy2(library, self.directory)
        else:
            os.symlink(program, self.program)

    def test_executable_path_with_spaces(self):
        command = self.program[:-4] if os.name == 'nt' else self.program
        result = Prog(command, '--fstrikethrough').to_html('~~hello~~\n')
        result[1] = result[1].replace('\r\n', '\n')
        self.assertEqual(result, [0, '<p><del>hello</del></p>\n', b''])

    def test_command_options_override_defaults(self):
        path = self.directory + os.pathsep + os.environ.get('PATH', '')
        with patch.dict(os.environ, {'PATH': path}):
            result = Prog('md2html --ftables', '--fstrikethrough').to_html(
                '~~hello~~\n')
        result[1] = result[1].replace('\r\n', '\n')
        self.assertEqual(result, [0, '<p>~~hello~~</p>\n', b''])


if __name__ == '__main__':
    program = os.path.abspath(sys.argv.pop(1))
    if os.name == 'nt' and not program.lower().endswith('.exe'):
        program += '.exe'
    unittest.main()
