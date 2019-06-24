import os.path
import sys


def get_cmake_exe_path():
    return os.path.join(get_cmake_dir(), 'Win32/bin/cmake.exe')


def get_cmake_dir():
    return os.path.join(get_tools_path(), 'CMake')


def get_script_dir():
    """
    Returns the path to the CryVersionSelector script folder, based on the location of this file.
    """
    if getattr(sys, 'frozen', False):
        # Note the scripts are in CryVersionSelector root while the frozen binaries are deployed
        script_dir = os.path.join(os.path.dirname(sys.executable), '../..')
    else:
        script_dir = os.path.dirname(__file__)
    return os.path.abspath(script_dir)


def get_tools_path():
    """
    Returns the path to the CRYENGINE Tools folder, based on the location of this file.
    """
    return os.path.abspath(os.path.join(get_script_dir(), '..'))


def get_engine_path():
    """
    Returns the path to the CRYENGINE folder, based on the location of this file.
    """
    return os.path.abspath(os.path.join(get_script_dir(), '..', '..'))


def get_exec_dir():
    """
    Returns the path to the current running script or frozen executable directory
    """
    if getattr(sys, 'frozen', False):
        dir = os.path.dirname(sys.executable)
    else:
        dir = os.path.dirname(__file__)
    return os.path.abspath(dir)