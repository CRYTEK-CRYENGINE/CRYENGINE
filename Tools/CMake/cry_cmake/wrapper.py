#!/usr/bin/env python3

import os.path
import sys
import importlib.machinery

# Workaround so it can be imported in cryselect.py
import os
import subprocess
import tkinter as tk
from tkinter import ttk

if __name__ == '__main__':
    if getattr( sys, 'frozen', False ):
        scriptpath = sys.executable
    else:
        scriptpath = __file__
    path = os.path.join(os.path.dirname(os.path.realpath(scriptpath)), 'Tools', 'CMake', 'cry_cmake')

    sys.path.insert(0, os.path.abspath(path))
    cry_cmake = importlib.machinery.SourceFileLoader('cry_cmake',os.path.join(path, 'cry_cmake.py')).load_module()
    cry_cmake.main()

