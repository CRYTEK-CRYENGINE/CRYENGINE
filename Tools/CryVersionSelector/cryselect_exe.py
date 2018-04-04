#!/usr/bin/env python3

import os.path
import sys
import importlib.machinery

# Workaround so it can be imported in cryselect.py
import json
import win32api, win32con
from win32com.shell import shell, shellcon
import ctypes
import uuid
import tkinter as tk
from tkinter import ttk
from tkinter import filedialog

if __name__ == '__main__':
    if getattr( sys, 'frozen', False ):
        scriptpath = sys.executable
    else:
        scriptpath = __file__
    path = os.path.dirname(os.path.realpath(scriptpath))

    sys.path.insert(0, os.path.abspath(path))
    cryselect = importlib.machinery.SourceFileLoader('cryselect',os.path.join(path, 'cryselect.py')).load_module()
    cryselect.main()