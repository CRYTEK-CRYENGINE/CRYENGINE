#!/usr/bin/env python3
"""
Script for freezing 'download_sdks.py'.
To freeze run::

    py -3 -m py2exe -d . -b 0 dl_sdks_exe.py
    rename dl_sdks_exe.exe download_sdks.exe
"""
import importlib.machinery
from urllib import request  # Workaround so it can be imported in download_sdks.py


if __name__ == '__main__':
    dl_sdks = importlib.machinery.SourceFileLoader('dl_sdks','download_sdks.py').load_module()
    dl_sdks.main()

