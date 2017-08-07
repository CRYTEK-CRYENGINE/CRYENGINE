import os
import sys
import subprocess
import tkinter as tk
from tkinter import ttk

CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))

#CRYENGINE_DIR = os.path.abspath(os.path.join(CURRENT_DIR,'../../../'))
CRYENGINE_DIR = os.getcwd()
CMAKE_DIR = os.path.abspath(os.path.join(CRYENGINE_DIR,'Tools/CMake'))
CMAKE_EXE = os.path.abspath(os.path.join(CMAKE_DIR,'Win32/bin/cmake.exe'))
CMAKE_GUI_EXE = os.path.abspath(os.path.join(CMAKE_DIR,'Win32/bin/cmake-gui.exe'))

CONFIGS = [
    {
        'title':'Visual Studio 2015 Win64',
        'cmake_toolchain': 'toolchain\windows\WindowsPC-MSVC.cmake',
        'cmake_generator': 'Visual Studio 14 2015 Win64',
        'cmake_builddir': 'solutions_cmake/win64',
    },
    {
        'title':'Visual Studio 2015 Win32',
        'cmake_toolchain': 'toolchain\windows\WindowsPC-MSVC.cmake',
        'cmake_generator': 'Visual Studio 14 2015',
        'cmake_builddir': 'solutions_cmake/win32',
    },
#    {
#        'title':'Visual Studio 2015 Android Nsight Tegra',
#        'cmake_toolchain': 'toolchain\android\Android-Nsight.cmake',
#        'cmake_generator': 'Visual Studio 14 2015 ARM',
#        'cmake_builddir': 'solutions_cmake/android',
#    },

#Visual Studio 15 2017
    {
        'title':'Visual Studio 2017 Win64',
        'cmake_toolchain': 'toolchain\windows\WindowsPC-MSVC.cmake',
        'cmake_generator': 'Visual Studio 15 2017 Win64',
        'cmake_builddir': 'solutions_cmake/win64',
    },
    {
        'title':'Visual Studio 2017 Win32',
        'cmake_toolchain': 'toolchain\windows\WindowsPC-MSVC.cmake',
        'cmake_generator': 'Visual Studio 15 2017',
        'cmake_builddir': 'solutions_cmake/win32',
    },
#    {
#        'title':'Visual Studio 2017 Android Nsight Tegra',
#        'cmake_toolchain': 'toolchain\android\Android-Nsight.cmake',
#        'cmake_generator': 'Visual Studio 15 2017 ARM',
#        'cmake_builddir': 'solutions_cmake/android',
#    }
]


def cmake_configure(generator, srcdir, builddir, cmakeexe=CMAKE_EXE, options=[],
             toolchain=None):

    srcdir = srcdir.replace('\\','/')
    builddir = builddir.replace('\\', '/')

    cmake_command = ['\"'+cmakeexe+'\"']

    cmake_command.append('-Wno-dev')

    if toolchain:
        toolchain = toolchain.replace('\\', '/')
        cmake_command.append('-DCMAKE_TOOLCHAIN_FILE=\"'+toolchain+'\"')

    cmake_command.append('\"'+srcdir+'\"')
    cmake_command.append('-B'+'\"'+builddir+'\"')

    cmake_command.append('-G\"'+generator+'\"')

    cmake_command.extend(options)


    print(' '.join(cmake_command))

    #ret = subprocess.call(cmake_command,shell=True)
    cmd = ' '.join(cmake_command)
    os.system("start /wait cmd /c \""+cmd+ '\"')

    # Start cmake-gui application
    subprocess.Popen([CMAKE_GUI_EXE,'-H'+srcdir,'-B'+builddir])
    sys.exit(0)


class Application(tk.Frame):
    def __init__(self, master=None):
        super().__init__(master)
        self.parent = master
        self.parent.title("CRYENGINE CMake Project Generator")
        self.parent.minsize(300,100);
        self.pack()
        self.create_widgets()

    def create_widgets(self):

        tk.Label(self, text="Generate Configuration: ").pack()

        self.newselection = ''
        self.box_value = tk.StringVar()
        self.configs_box = ttk.Combobox(self, textvariable=self.box_value,width=40)
        #self.configs_box.minsize(300, 100);

        config_list = []
        for config in CONFIGS:
            config_list.append(config['title'])
        self.configs_box['values'] = config_list

        self.configs_box.current(0)
        #self.configs_box.bind("<<ComboboxSelected>>", self.newselection)
        #self.configs_box.grid(column=0, row=0)
        #self.configs_box.pack(side="top")
        self.configs_box.pack()

        self.generate = tk.Button(self)
        self.generate["text"] = "Generate Solution"
        self.generate["command"] = self.generate_cmd
        #self.generate.pack(side="top")
        self.generate.pack()

        #self.quit = tk.Button(self, text="QUIT", fg="red",command=root.destroy)
        #self.quit.pack(side="bottom")

    def say_hi(self):
        print("hi there, everyone!")

    def generate_cmd(self):
        current = self.configs_box.current()
        config = CONFIGS[current]
        cmake_configure(
            generator=config['cmake_generator'],
            srcdir = CRYENGINE_DIR,
            builddir = os.path.join(CRYENGINE_DIR,config['cmake_builddir']),
            toolchain = os.path.join(CMAKE_DIR,config['cmake_toolchain'])
            )

    def newselection(self):
        print ('selected')

    def combo(self):
        self.newselection = CONFIGS[0]
        self.box_value = tk.StringVar()
        self.box = ttk.Combobox(self, textvariable=self.box_value)
        self.box['values'] = CONFIGS
        self.box.current(0)
        self.box.bind("<<ComboboxSelected>>", self.newselection)
        self.box.grid(column=0, row=0)

iconfile = "icon.ico"
if not hasattr(sys, "frozen"):
    iconfile = os.path.join(os.path.dirname(__file__), iconfile)
else:
    iconfile = os.path.join(sys.prefix, iconfile)

root = tk.Tk()
root.iconbitmap(iconfile)
app = Application(master=root)
app.mainloop()
