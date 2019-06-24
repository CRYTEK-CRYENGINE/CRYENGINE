#!/usr/bin/env python3

import os.path
import sys
import importlib.machinery

# Workaround so it can be imported in cryselect.py
import json
import win32api
import win32con
from win32com.shell import shell, shellcon
import ctypes
import uuid
import tkinter as tk
from tkinter import ttk
from tkinter import filedialog
import crypath

if __name__ == '__main__':
    path = crypath.get_script_dir()

    sys.path.insert(0, path)
    cryselect = importlib.machinery.SourceFileLoader(
        'cryselect', os.path.join(path, 'cryselect.py')).load_module()
    cryselect.main()
