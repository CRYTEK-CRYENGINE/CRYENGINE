#!/usr/bin/env python3
## Using Python 3.5
import os
import sys
import subprocess
import tkinter as tk
from tkinter import ttk

import re
import pathlib
import asyncio
from asyncio.subprocess import PIPE

## If this python script is in its proper location post git setup, then engine root is just 3 folders away.
## Otherwise default to the directory that script was run from.
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if (SCRIPT_DIR.endswith("Tools\CMake\cry_cmake")):
    CRYENGINE_DIR = os.path.abspath(os.path.join(SCRIPT_DIR,'../../../'))
else:
    CRYENGINE_DIR = os.getcwd()

CMAKE_DIR = os.path.abspath(os.path.join(CRYENGINE_DIR,'Tools','CMake'))
CMAKE_EXE = os.path.abspath(os.path.join(CMAKE_DIR,'Win32','bin','cmake.exe'))
CMAKE_GUI_EXE = os.path.abspath(os.path.join(CMAKE_DIR,'Win32','bin','cmake-gui.exe'))
#CODE_SDKS_DIR = os.path.abspath(os.path.join(CRYENGINE_DIR,'Code','SDKs'))

CONFIGS = [
#Visual Studio 2015 Express
    {
        'title':'Visual Studio 2015 Express Win64',
        'cmake_toolchain': 'toolchain/windows/WindowsPC-MSVC.cmake',
        'cmake_generator': 'Visual Studio 14 2015 Win64',
        'cmake_builddir': 'solutions/win64',
        'compiler':{'key_path': r'\WDExpress.DTE.14.0'}
    },
    {
        'title':'Visual Studio 2015 Express Win32',
        'cmake_toolchain': 'toolchain/windows/WindowsPC-MSVC.cmake',
        'cmake_generator': 'Visual Studio 14 2015',
        'cmake_builddir': 'solutions/win32',
        'compiler':{'key_path': r'\WDExpress.DTE.14.0'}
    },
#Visual Studio 2015
    {
        'title':'Visual Studio 2015 Win64',
        'cmake_toolchain': r'toolchain\windows\WindowsPC-MSVC.cmake',
        'cmake_generator': 'Visual Studio 14 2015 Win64',
        'cmake_builddir': 'solutions_cmake/win64',
        'compiler': { 'key_path': r'\VisualStudio.DTE.14.0' }
    },
    {
        'title':'Visual Studio 2015 Win32',
        'cmake_toolchain': r'toolchain\windows\WindowsPC-MSVC.cmake',
        'cmake_generator': 'Visual Studio 14 2015',
        'cmake_builddir': 'solutions_cmake/win32',
        'compiler': { 'key_path': r'\VisualStudio.DTE.14.0' }
    },
#    {
#        'title':'Visual Studio 2015 Android Nsight Tegra',
#        'cmake_toolchain': r'toolchain\android\Android-Nsight.cmake',
#        'cmake_generator': 'Visual Studio 14 2015 ARM',
#        'cmake_builddir': 'solutions_cmake/android',
#    },

#Visual Studio 2017
    {
        'title':'Visual Studio 2017 Win64',
        'cmake_toolchain': r'toolchain\windows\WindowsPC-MSVC.cmake',
        'cmake_generator': 'Visual Studio 15 2017 Win64',
        'cmake_builddir': 'solutions_cmake/win64',
        'compiler': { 'key_path': r'\VisualStudio.DTE.15.0' }
    },
    {
        'title':'Visual Studio 2017 Win32',
        'cmake_toolchain': r'toolchain\windows\WindowsPC-MSVC.cmake',
        'cmake_generator': 'Visual Studio 15 2017',
        'cmake_builddir': 'solutions_cmake/win32',
        'compiler': { 'key_path': r'\VisualStudio.DTE.15.0' }
    },
#    {
#        'title':'Visual Studio 2017 Android Nsight Tegra',
#        'cmake_toolchain': r'toolchain\android\Android-Nsight.cmake',
#        'cmake_generator': 'Visual Studio 15 2017 ARM',
#        'cmake_builddir': 'solutions_cmake/android',
#    }
]

def valid_configs():
    try:
        import winreg
        def valid_config(c):
            try:
                registry = winreg.ConnectRegistry(None, winreg.HKEY_CLASSES_ROOT)
                key = winreg.OpenKey(registry, c['compiler']['key_path'])
                return True
            except:
                return False
        return [c for c in CONFIGS if valid_config(c)]
    except ImportError:
        return CONFIGS

CONFIGS = valid_configs()

def configure_window(win):
    win.title("CRYENGINE CMake Project Generator")
    width = 370
    height = 93
    x = (win.winfo_screenwidth() // 2) - (width // 2)
    y = (win.winfo_screenheight() // 2) - (height // 2)
    win.geometry('{}x{}+{}+{}'.format(width, height, x, y))
    win.minsize(width,height)
    
def cmake_configure(generator, srcdir, builddir, cmakeexe=CMAKE_EXE, options=[],
             toolchain=None, build=True):

    srcdir = srcdir.replace('\\','/')
    builddir = builddir.replace('\\', '/')

    cmake_command = ['\"'+cmakeexe+'\"']

    cmake_command.append('-Wno-dev')

	## Would run toolchain twice, already handled in CMake.
    ##if toolchain:
    ##    toolchain = toolchain.replace('\\', '/')
    ##    cmake_command.append('-DCMAKE_TOOLCHAIN_FILE=\"'+toolchain+'\"')

    cmake_command.append('\"'+srcdir+'\"')
    cmake_command.append('-B'+'\"'+builddir+'\"')

    cmake_command.append('-G\"'+generator+'\"')
    
    cmake_command.append('-DCMAKE_MODE:INTERNAL=CLI') ## To tell if it was triggered via GUI or CLI
    
    cmake_command.extend(options)

    rc=0
    if(build):
        cmd = ' '.join(cmake_command)
        print('Running command:\n'+cmd+'\n===============================================')
        
        try:
            rc, *output = loop.run_until_complete(read_and_display(cmd))
            loop.close()
        except Exception as ex:
            print(ex)
            input("==================== ERROR ====================\nFailed to start build! Press Enter to quit...\n")
            sys.exit(0)

    if(rc == 0):
        if(build):
            input("=================== SUCCESS ===================\nBuild done! Press Enter to quit...\n")

        # Start cmake-gui application
        subprocess.Popen([CMAKE_GUI_EXE,'-H'+srcdir,'-B'+builddir])
    else:
        input("==================== ERROR ====================\nFailed to build! Press Enter to quit...\n")

    sys.exit(0)

class Application(tk.Frame):
    def __init__(self, master=None):
        super().__init__(master)
        self.parent = master
        self.pack()
        self.create_widgets()

    def create_widgets(self):

        tk.Label(self, text="Configuration: ").grid(column=0, row=1)

        self.newselection = ''
        self.box_value = tk.StringVar()
        self.configs_box = ttk.Combobox(self, textvariable=self.box_value,width=40)
        #self.configs_box.minsize(300, 100);

        config_list = []
        for config in CONFIGS:
            config_list.append(config['title'])
        self.configs_box['values'] = config_list

        i = 0
        try:
            with open(os.path.expandvars(r'%APPDATA%\Crytek\CryENGINE\cry_cmake.cfg'), 'r') as f:
                last_choice = f.read()
            for c in CONFIGS:
                if c['title'] == last_choice:
                    i = CONFIGS.index(c)
        except:
            pass
        self.configs_box.current(i)
        #self.configs_box.bind("<<ComboboxSelected>>", self.newselection)
        self.configs_box.grid(column=0, row=2)
        #self.configs_box.pack(side="top")
        #self.configs_box.pack()

        self.generate = tk.Button(self)
        self.generate["text"] = "              Generate Solution"
        self.generate["command"] = self.generate_cmd
        #self.generate.pack(side="top")
        self.generate.grid(column=0, row=0, sticky="W")

        self.gui = tk.Button(self, text="GUI or", bg="#2aeaea", command= lambda: self.generate_cmd(True))
        #self.gui.pack(side="top")
        self.gui.grid(column=0, row=0, sticky="W")

        #self.quit = tk.Button(self, text="QUIT", fg="red",command=root.destroy)
        #self.quit.pack(side="bottom")

        self.parent.attributes('-topmost', 1)
        self.parent.attributes('-topmost', 0)
    
    def generate_cmd(self, skip=False):
        current = self.configs_box.current()
        config = CONFIGS[current]
        fPath=os.path.expandvars(r'%APPDATA%\Crytek\CryENGINE\cry_cmake.cfg')
        pathlib.Path(os.path.dirname(fPath)).mkdir(parents=True, exist_ok=True) 
        with open(fPath, 'w') as f:
            f.write(config['title'])
        self.parent.destroy()
        cmake_configure(
            generator=config['cmake_generator'],
            srcdir = CRYENGINE_DIR,
            builddir = os.path.join(CRYENGINE_DIR,config['cmake_builddir']),
            toolchain = os.path.join(CMAKE_DIR,config['cmake_toolchain']),
            build = not skip
        )

def main():
    print('Detected engine root as: "%s"' % CRYENGINE_DIR)

    iconfile = "icon.ico"
    if not hasattr(sys, "frozen"):
        iconfile = os.path.join(SCRIPT_DIR, iconfile)
    else:
        iconfile = os.path.join(sys.prefix, iconfile)

    root = tk.Tk()
    configure_window(root)
    root.iconbitmap(iconfile)
    app = Application(master=root)
    app.mainloop()

##
## Shameless copy-paste & merge from
## https://stackoverflow.com/questions/17190221/subprocess-popen-cloning-stdout-and-stderr-both-to-terminal-and-variables/25960956#25960956
## https://stackoverflow.com/questions/16260061/reading-a-file-with-a-specified-delimiter-for-newline
## https://stackoverflow.com/questions/19600475/how-to-read-records-terminated-by-custom-separator-from-file-in-python
## For lack of a faster/cleaner solution.

# Run the event loop
if os.name == 'nt':
    loop = asyncio.ProactorEventLoop() # for subprocess' pipes on Windows
    asyncio.set_event_loop(loop)
else:
    loop = asyncio.get_event_loop()
        
@asyncio.coroutine
def read_stream_and_display(stream, display):
    """Read from stream line by line (preserving format) until EOF, display, and capture the lines.
    """
    output = []
    buf = b''
    EOL_REGEX=b'\r\n|\r|\n'
    bufsize=4096
    
    while True:
        newbuf = yield from stream.read(bufsize)
        if not newbuf:
            output.append(buf)
            display(buf)
            break
        else:
            buf+=newbuf
            lines = re.split(b'('+EOL_REGEX+b')', buf) ## Keep \r & \n formatting!
            for line in lines[:-1]:
                output.append(line)
                display(line)
            buf = lines[-1]

    return b''.join(output)

@asyncio.coroutine
def read_and_display(*cmd):
    """Capture cmd's stdout, stderr while displaying them as they arrive
    (line by line).
    """
    # Start process
    process = yield from asyncio.create_subprocess_shell(*cmd,
            stdout=PIPE, stderr=PIPE)

    # Read child's stdout/stderr concurrently (capture and display)
    try:
        stdout, stderr = yield from asyncio.gather(
            read_stream_and_display(process.stdout, sys.stdout.buffer.write),
            read_stream_and_display(process.stderr, sys.stderr.buffer.write))
    except Exception:
        process.kill()
        raise
    finally:
        # Wait for the process to exit
        rc = yield from process.wait()
    return rc, stdout, stderr
	