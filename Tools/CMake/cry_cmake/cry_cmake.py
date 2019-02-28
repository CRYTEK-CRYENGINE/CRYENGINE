import os
import sys
import subprocess
import tkinter as tk
from tkinter import ttk


CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))

#CRYENGINE_DIR = os.path.abspath(os.path.join(CURRENT_DIR,'../../../'))
CRYENGINE_DIR = os.getcwd()
CMAKE_DIR = os.path.abspath(os.path.join(CRYENGINE_DIR,'Tools','CMake'))
CMAKE_EXE = os.path.abspath(os.path.join(CMAKE_DIR,'Win32','bin','cmake.exe'))
CMAKE_GUI_EXE = os.path.abspath(os.path.join(CMAKE_DIR,'Win32','bin','cmake-gui.exe'))
CODE_SDKS_DIR = os.path.abspath(os.path.join(CRYENGINE_DIR,'Code','SDKs'))
SDK_DOWNLOAD_EXE = os.path.abspath(os.path.join(CRYENGINE_DIR,'download_sdks.exe'))

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

def center_window(win):
    win.update_idletasks()
    width = win.winfo_width()
    height = win.winfo_height()
    x = (win.winfo_screenwidth() // 2) - (width // 2)
    y = (win.winfo_screenheight() // 2) - (height // 2)
    win.geometry('{}x{}+{}+{}'.format(width, height, x, y))

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

    def generate_cmd(self):
        current = self.configs_box.current()
        config = CONFIGS[current]
        with open(os.path.expandvars(r'%APPDATA%\Crytek\CryENGINE\cry_cmake.cfg'), 'w') as f:
            f.write(config['title'])
        self.parent.destroy()
        cmake_configure(
            generator=config['cmake_generator'],
            srcdir = CRYENGINE_DIR,
            builddir = os.path.join(CRYENGINE_DIR,config['cmake_builddir']),
            toolchain = os.path.join(CMAKE_DIR,config['cmake_toolchain'])
            )

class dlgMissingSDKs(tk.Frame):
    def __init__(self, master=None):
        super().__init__(master)
        self.parent = master
        self.parent.title("CRYENGINE CMake Project Generator")
        self.parent.minsize(300,100);
        self.pack()
        self.create_widgets()

    def create_widgets(self):
        self.lblText = tk.Label(self, text="Missing 3rd-party SDKs folder at \n'" + CODE_SDKS_DIR + "'.\n")
        self.lblText.grid(row=0, column=0, columnspan=1)

        self.frmButtons = tk.Frame(self)
        self.frmButtons.grid(row=1, column=0, columnspan=1)

        self.btnDownload = tk.Button(self.frmButtons)
        self.btnDownload["text"] = "Download SDKs"
        self.btnDownload["command"] = self.handleDownload
        self.btnDownload.grid(row=0, column=1, padx=10, pady=10)

        self.btnContinue = tk.Button(self.frmButtons)
        self.btnContinue["text"] = "Continue (Skip)"
        self.btnContinue["command"] = self.handleContinue
        self.btnContinue.grid(row=0, column=2, padx=10, pady=10)

        self.btnCancel = tk.Button(self.frmButtons)
        self.btnCancel["text"] = "Cancel (Quit)"
        self.btnCancel["command"] = self.handleCancel
        self.btnCancel.grid(row=0, column=3, padx=10, pady=10)



    def handleDownload(self):
        self.parent.destroy()

        try:
            os.system("\"" + SDK_DOWNLOAD_EXE + "\"")
        except:
            self.showDialog("CRYENGINE Error!", ("Cannot execute download_sdks.exe at:\n(%s)" % SDK_DOWNLOAD_EXE))

        if not os.path.isdir(CODE_SDKS_DIR):
            self.showDialog("CRYENGINE Error!", "SDKs were not downloaded (download_sdks.exe failed or was closed too early?).")
            sys.exit()

        self.quit()

    def showDialog(self, title, msg):
        global iconfile
        popup = tk.Tk()
        popup.iconbitmap(iconfile)
        popup.title(title)
        ttk.Label(popup, text=msg).pack(pady=10, padx=10)
        btnOk = ttk.Button(popup, text="Ok", command = popup.destroy)
        btnOk.pack(pady=20, padx=10)
        center_window(popup)
        self.mainloop()

    def handleContinue(self):
        self.parent.destroy()

    def handleCancel(self):
        self.parent.destroy()
        sys.exit()

iconfile = "icon.ico"
if not hasattr(sys, "frozen"):
    iconfile = os.path.join(os.path.dirname(__file__), iconfile)
else:
    iconfile = os.path.join(sys.prefix, iconfile)

# Check Code/SDKs folder exists before proceeding
if not os.path.isdir(CODE_SDKS_DIR):
    dlgRoot = tk.Tk()
    dlgRoot.iconbitmap(iconfile)
    dialog = dlgMissingSDKs(master=dlgRoot)
    center_window(dlgRoot)
    dialog.mainloop()

root = tk.Tk()
root.iconbitmap(iconfile)
app = Application(master=root)
center_window(root)
app.mainloop()
