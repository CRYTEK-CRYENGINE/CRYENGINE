#!/usr/bin/env python3
"""
Script for freezing 'p4v_uncrustify.py'.
To freeze run:
    pyinstaller --distpath=. p4v_uncrustify.spec
"""
import imp
import os
import sys


def main():
    """
    Entry point.
    """
    script_path = os.path.abspath(os.path.dirname(sys.argv[0]))
    name = 'p4v_uncrustify'
    fd_module, path_name, description = imp.find_module(name, [script_path])
    try:
        module = imp.load_module(name, fd_module, path_name, description)
    finally:
        if fd_module:
            fd_module.close()
    return module.main()


if __name__ == '__main__':
    sys.exit(main())
