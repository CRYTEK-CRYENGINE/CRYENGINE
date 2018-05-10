#!/usr/bin/env python3

import sys
import os.path
import importlib.machinery
import importlib.util

# Workaround so it can be imported in cryrun.py
import json
import win32api, win32con, win32file
from win32com.shell import shell
import ctypes
import tkinter as tk
from tkinter import ttk
from tkinter import filedialog
import configparser
import admin
import distutils.dir_util, distutils.file_util
import winreg
import uuid

if __name__ == '__main__':
    if getattr( sys, 'frozen', False ):
        scriptpath = sys.executable
    else:
        scriptpath = __file__
    path = os.path.dirname(os.path.realpath(scriptpath))

    sys.path.insert(0, os.path.abspath(path))
    cryrun = importlib.machinery.SourceFileLoader('cryrun',os.path.join(path, 'cryrun.py')).load_module()
    cryrun.main()