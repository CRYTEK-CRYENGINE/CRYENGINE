#!/usr/bin/env python3
## Using Python 3.5
import sys
import importlib.machinery

# Workaround so it can be imported in cryselect.py
import os
import subprocess
import tkinter as tk
from tkinter import ttk

import re
import pathlib
import asyncio
from asyncio.subprocess import PIPE

##
## Quickie to unbuffer python.
##
class Unbuffered(object):
   def __init__(self, stream):
       self.stream = stream
   def write(self, data):
       self.stream.write(data)
       self.stream.flush()
   def writelines(self, datas):
       self.stream.writelines(datas)
       self.stream.flush()
   def __getattr__(self, attr):
       return getattr(self.stream, attr)

sys.stdout = Unbuffered(sys.stdout)
sys.stdout.buffer = Unbuffered(sys.stdout.buffer)

if __name__ == '__main__':
    if getattr( sys, 'frozen', False ):
        scriptpath = sys.executable
    else:
        scriptpath = __file__
    path = os.path.dirname(os.path.realpath(scriptpath))

    ## If this python script is in its proper location post git setup, then don't add path again.
    if (not path.endswith("Tools\CMake\cry_cmake")):
        path = os.path.join(path, 'Tools', 'CMake', 'cry_cmake')
    
    sys.path.insert(0, os.path.abspath(path))
    cry_cmake = importlib.machinery.SourceFileLoader('cry_cmake',os.path.join(path, 'cry_cmake.py')).load_module()

    try:
        cry_cmake.main()
    except Exception as ex:
        print ("ERR: "+str(ex))
        input("Press Enter to quit...\n")
        exit(1)

