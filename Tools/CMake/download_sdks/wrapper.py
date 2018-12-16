#!/usr/bin/env python3
## Using Python 3.5
import os
import sys
import importlib.machinery

# Workaround so it can be imported in cryselect.py
import json
from sys import exit 
from urllib import request

## Slightly different input/output here.
CMAKE_GUI = False if (len(sys.argv) > 1 or sys.stdout.isatty()) else True

##
## Quickie to unbuffer python, required for live CMake output.
##
class Unbuffered(object):
   def __init__(self, stream):
        self.stream = stream
   def write(self, data):
        if CMAKE_GUI:
            data = data.rstrip("\n\r")
        self.stream.write(data)
        self.stream.flush()
   def writelines(self, datas):
        if CMAKE_GUI:
            data = data.rstrip("\n\r")
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
    if (not path.endswith("Tools\CMake\download_sdks")):
        path = os.path.join(path, 'Tools', 'CMake', 'download_sdks')
    
    sys.path.insert(0, os.path.abspath(path))
    download_sdks = importlib.machinery.SourceFileLoader('download_sdks',os.path.join(path, 'download_sdks.py')).load_module()
    
    try:
        download_sdks.main()
    except Exception as ex:
        print ("ERR: "+str(ex))
        ## Output is to file when running any CMake.
        if sys.stdout.isatty():
            input("Press Enter to quit...\n")
           
        exit(1)
        
        
        