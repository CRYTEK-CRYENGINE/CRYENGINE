#!/usr/bin/env python3
"""
Cryselect is distributed with the web launcher - there exists only one copy of the application on the system.
It is intended to called by the web launcher and Windows file extension integration.
Cryselect is responsible for maintaining the project registry, and forwarding engine commands to cryrun.
"""

import argparse
import os.path
import sys
import json

import shutil
import subprocess
import uuid

import tkinter as tk
from tkinter import ttk
from tkinter import filedialog

import cryproject
import cryregistry
import backup_project

HAS_WIN_MODULES = True
try:
    import win32api
    import win32con
    from win32com.shell import shell
    import pywintypes
    import ctypes
    MESSAGEBOX = ctypes.windll.user32.MessageBoxW
except ImportError:
    HAS_WIN_MODULES = False

def command_title(args):
    """
    Checks the specified args and returns the command that was run.
    Returns an empty string if no command is in the args.
    """
    return {
        'upgrade': 'Upgrade project',
        'projgen': 'Generate solution',
        'cmake-gui': 'Open CMake GUI',
        'edit': 'Launch editor',
        'open': 'Launch game',
        'server': 'Launch server',
        'package': 'Package for release',
        'switch': 'Switch engine version',
        'metagen': 'Generate/repair metadata',
        'backup': 'Create a backup of the project'
    }.get(args.command, '')

#--- errors

SUBPROCESS_NO_STDERR = 'An unexpected error occurred. Press OK to review the output log.'

def error_project_not_found(args):
    """
    Error to specify that the .cryproject file couldn't be found.
    """
    message = "'{}' not found.\n".format(args.project_file)
    if not args.silent and HAS_WIN_MODULES:
        MESSAGEBOX(None, message, command_title(args), win32con.MB_OK | win32con.MB_ICONERROR)
    else:
        sys.stderr.write(message)
    sys.exit(600)

def error_engine_not_found(args):
    """
    Error to specify that the .cryengine file couldn't be found.
    """
    message = "'{}' not found.\n".format(args.engine_file)
    if not args.silent and HAS_WIN_MODULES:
        MESSAGEBOX(None, message, command_title(args), win32con.MB_OK | win32con.MB_ICONERROR)
    else:
        sys.stderr.write(message)
    sys.exit(600)

def error_project_json_decode(args):
    """
    Error to specify that the json file is corrupt and couldn't be parsed.
    """
    message = "Unable to parse '{}'.\n".format(args.project_file)
    if not args.silent and HAS_WIN_MODULES:
        MESSAGEBOX(None, message, command_title(args), win32con.MB_OK | win32con.MB_ICONERROR)
    else:
        sys.stderr.write(message)
    sys.exit(601)

def error_engine_json_decode(engine_file, silent):
    """
    Error to specify that the json file is corrupt and couldn't be parsed.
    """
    message = "Unable to parse '{}'.\n".format(engine_file)
    if not silent and HAS_WIN_MODULES:
        MESSAGEBOX(None, message, 'Unable to parse cryengine file', win32con.MB_OK | win32con.MB_ICONERROR)
    else:
        sys.stderr.write(message)
    sys.exit(604)

def error_engine_path_not_found(args, engine_version):
    """
    Error to specify that the engine path could not be found,
    because the engine was not registered.
    """
    message = "CryEngine '{}' has not been registered locally.\n".format(engine_version)
    if not args.silent and HAS_WIN_MODULES:
        MESSAGEBOX(None, message, command_title(args), win32con.MB_OK | win32con.MB_ICONERROR)
    else:
        sys.stderr.write(message)
    sys.exit(602)

def error_missing_windows_modules(args, command):
    """
    Error to specify that a command is not supported on other platforms than Windows.
    """
    message = "Missing windows modules! The command '{}' is not supported on non-windows platforms.".format(command)
    if not args.silent and HAS_WIN_MODULES:
        MESSAGEBOX(None, message, command_title(args), win32con.MB_OK | win32con.MB_ICONERROR)
    else:
        sys.stderr.write(message)
    sys.exit(603)

def error_engine_tool_not_found(args, path):
    """
    Error to specify that a specific engine tool couldn't be found.
    For example cryrun.exe.
    """
    message = "'{}' not found. Please re-register CRYENGINE version that includes the required tool.\n".format(path)
    if not args.silent and HAS_WIN_MODULES:
        MESSAGEBOX(None, message, command_title(args), win32con.MB_OK | win32con.MB_ICONERROR)
    else:
        sys.stderr.write(message)
    sys.exit(620)

def print_subprocess(cmd):
    """
    Prints a subprocess command in a readable format.
    """
    print(' '.join(map(lambda a: '"%s"' % a, cmd)))

#---

def get_python_path():
    """
    Get the path to Python on this machine.
    """
    if not getattr(sys, 'frozen', False):
        return sys.executable

    program = 'python'
    path = shutil.which(program)
    if path:
        return path

    path = 'C:\\'
    folders = [name for name in os.listdir(path) if name.lower().startswith('python3') and os.path.isdir(os.path.join(path, name))]
    if not folders:
        return program

    folders.sort()
    folders.reverse()
    return os.path.join(path, folders[0], program)

#--- UNINSTALL ---

def uninstall_integration():
    """
    Removes all entries from CryVersionSelector in the Windows registry.
    """
    if not HAS_WIN_MODULES:
        return

    for subkey in (os.path.join('Software', 'Classes', 'CrySelect.engine'), os.path.join('Software', 'Classes', 'CrySelect.project')):
        try:
            win32api.RegDeleteTree(win32con.HKEY_CURRENT_USER, subkey)
        except pywintypes.error:
            pass
        try:
            win32api.RegDeleteTree(win32con.HKEY_LOCAL_MACHINE, subkey)
        except pywintypes.error:
            pass

def cmd_uninstall(args):
    """
    Uninstalls the CryVersionSelector from this machine.
    Also unregisters all registered engines.
    """
    if HAS_WIN_MODULES:
        uninstall_integration()
    cryregistry.delete()

#--- INSTALL ---
#http://stackoverflow.com/questions/2123762/add-menu-item-to-windows-context-menu-only-for-specific-filetype
#http://sbirch.net/tidbits/context_menu.html
#http://code.activestate.com/recipes/286159-using-ctypes-to-manipulate-windows-registry-and-to/

def cmd_install(args):
    """
    Installs the CryVersionSelector on this machine.
    """
    if not HAS_WIN_MODULES:
        error_missing_windows_modules(args, "install")

    uninstall_integration()

    #---

    if getattr(sys, 'frozen', False):
        script_path = os.path.abspath(sys.executable)
        engine_commands = [
            ('add', 'Register engine', '"%s" add "%%1"' % script_path),
            ('1engine_gen', 'Generate engine solution', '"%s" engine_gen "%%1"' % script_path)
        ]

        # --- extended, action, title, command
        # The first collumn defines extended action. The associated commands will be
        # displayed only when the user right-clicks an object while also pressing the SHIFT key.
        # https://msdn.microsoft.com/en-us/library/cc144171(VS.85).aspx
        project_commands = [
            (False, 'edit', 'Launch editor', '"%s" edit "%%1"' % script_path),
            (False, '1open', 'Launch game', '"%s" open "%%1"' % script_path),
            (False, '2dedicated', 'Launch dedicated server', '"%s" server "%%1"' % script_path),
            (False, '3package', 'Package Build', '"%s" package "%%1"' % script_path),
            (False, '4metagen', 'Generate/repair metadata', '"%s" metagen "%%1"' % script_path),
            (False, '5projgen', 'Generate solution', '"%s" projgen "%%1"' % script_path),
            (False, '6cmake-gui', 'Open CMake GUI', '"%s" cmake-gui "%%1"' % script_path),
            (False, '7switch', 'Switch engine version', '"%s" switch "%%1"' % script_path),
            (False, '8backup', 'Backup project', '"%s" backup "%%1"' % script_path)
        ]
    else:
        script_path = os.path.abspath(__file__)
        python_path = get_python_path()
        engine_commands = [
            ('add', 'Register engine', '"%s" "%s" add "%%1"' % (python_path, script_path)),
            ('1engine_gen', 'Generate engine solution', '"%s" "%s" engine_gen "%%1"' % (python_path, script_path))
        ]

        project_commands = [
            (False, 'edit', 'Launch editor', '"%s" "%s" edit "%%1"' % (python_path, script_path)),
            (False, '1open', 'Launch game', '"%s" "%s" open "%%1"' % (python_path, script_path)),
            (False, '2dedicated', 'Launch dedicated server', '"%s" "%s" server "%%1"' % (python_path, script_path)),
            (False, '3package', 'Package Build', '"%s" "%s" package "%%1"' % (python_path, script_path)),
            (False, '4metagen', 'Generate/repair metadata', '"%s" "%s" metagen "%%1"' % (python_path, script_path)),
            (False, '5projgen', 'Generate solution', '"%s" "%s" projgen "%%1"' % (python_path, script_path)),
            (False, '6cmake-gui', 'Open CMake GUI', '"%s" "%s" cmake-gui "%%1"' % (python_path, script_path)),
            (False, '7switch', 'Switch engine version', '"%s" "%s" switch "%%1"' % (python_path, script_path)),
            (False, '8backup', 'Backup project', '"%s" "%s" backup "%%1"' % (python_path, script_path))
        ]

    #---

    key = False and win32con.HKEY_LOCAL_MACHINE or win32con.HKEY_CURRENT_USER
    hClassesRoot = win32api.RegOpenKeyEx(key, 'Software\\Classes')
    #https://msdn.microsoft.com/en-us/library/windows/desktop/cc144152(v=vs.85).aspx

    #--- .cryengine

    FriendlyTypeName = 'CryEngine version'
    ProgID = 'CrySelect.engine'
    AppUserModelID = 'CrySelect.engine'
    DefaultIcon = os.path.abspath(os.path.join(os.path.dirname(script_path), 'editor_icon.ico'))

    hProgID = win32api.RegCreateKey(hClassesRoot, ProgID)
    win32api.RegSetValueEx(hProgID, None, None, win32con.REG_SZ, FriendlyTypeName)
    win32api.RegSetValueEx(hProgID, 'AppUserModelID', None, win32con.REG_SZ, AppUserModelID)
    win32api.RegSetValueEx(hProgID, 'FriendlyTypeName', None, win32con.REG_SZ, FriendlyTypeName)
    win32api.RegSetValue(hProgID, 'DefaultIcon', win32con.REG_SZ, DefaultIcon)

    #---

    hShell = win32api.RegCreateKey(hProgID, 'shell')
    win32api.RegCloseKey(hProgID)

    for action, title, command in engine_commands:
        hAction = win32api.RegCreateKey(hShell, action)
        win32api.RegSetValueEx(hAction, None, None, win32con.REG_SZ, title)
        win32api.RegSetValueEx(hAction, 'Icon', None, win32con.REG_SZ, DefaultIcon)
        win32api.RegSetValue(hAction, 'command', win32con.REG_SZ, command)
        win32api.RegCloseKey(hAction)

    action = 'add'
    win32api.RegSetValueEx(hShell, None, None, win32con.REG_SZ, action)
    win32api.RegCloseKey(hShell)

    #---

    hCryProj = win32api.RegCreateKey(hClassesRoot, cryregistry.ENGINE_EXTENSION)
    win32api.RegSetValueEx(hCryProj, None, None, win32con.REG_SZ, ProgID)
    win32api.RegCloseKey(hCryProj)

    #--- .cryproject

    FriendlyTypeName = 'CryEngine project'
    ProgID = 'CrySelect.project'
    AppUserModelID = 'CrySelect.project'
    DefaultIcon = os.path.abspath(os.path.join(os.path.dirname(script_path), 'editor_icon16.ico'))

    hProgID = win32api.RegCreateKey(hClassesRoot, ProgID)
    win32api.RegSetValueEx(hProgID, None, None, win32con.REG_SZ, FriendlyTypeName)
    win32api.RegSetValueEx(hProgID, 'AppUserModelID', None, win32con.REG_SZ, AppUserModelID)
    win32api.RegSetValueEx(hProgID, 'FriendlyTypeName', None, win32con.REG_SZ, FriendlyTypeName)
    win32api.RegSetValue(hProgID, 'DefaultIcon', win32con.REG_SZ, DefaultIcon)

    #---

    hShell = win32api.RegCreateKey(hProgID, 'shell')
    win32api.RegCloseKey(hProgID)

    for extended, action, title, command in project_commands:
        hAction = win32api.RegCreateKey(hShell, action)
        win32api.RegSetValueEx(hAction, None, None, win32con.REG_SZ, title)
        win32api.RegSetValueEx(hAction, 'Icon', None, win32con.REG_SZ, DefaultIcon)
        win32api.RegSetValue(hAction, 'command', win32con.REG_SZ, command)
        if extended:
            win32api.RegSetValueEx(hAction, 'extended', None, win32con.REG_SZ, '')

        win32api.RegCloseKey(hAction)

    action = 'edit'
    win32api.RegSetValueEx(hShell, None, None, win32con.REG_SZ, action)
    win32api.RegCloseKey(hShell)

    #---

    hCryProj = win32api.RegCreateKey(hClassesRoot, '.cryproject')
    win32api.RegSetValueEx(hCryProj, None, None, win32con.REG_SZ, ProgID)
    win32api.RegCloseKey(hCryProj)

#--- ADD ---

def add_engines(*engine_files, silent):
    """
    Adds the collection of engines to the registered engines.
    """
    engine_registry = cryregistry.load_engines()

    added = []
    for engine_file in engine_files:
        if os.path.splitext(engine_file)[1] != cryregistry.ENGINE_EXTENSION:
            continue

        engine = cryproject.load(engine_file)
        if not engine:
            error_engine_json_decode(engine_file, silent)

        info = engine['info']
        engine_id = info['id']

        engine_data = {'uri': os.path.abspath(engine_file), 'info': info}
        prev = engine_registry.setdefault(engine_id, engine_data)
        if prev is engine_data or prev != engine_data:
            engine_registry[engine_id] = engine_data
            added.append(engine_id)

    if added:
        return cryregistry.save_engines(engine_registry, register_action=True, silent=silent)
    return 0

def cmd_add(args):
    """
    Adds the engine to the registered engines.
    """
    sys.exit(add_engines(*args.engine_files, silent=args.silent))

#--- REMOVE ---

def cmd_remove(args):
    """
    Removes the engine from the registery-file.
    """
    engine_registry = cryregistry.load_engines()

    removed = []
    for engine_file in args.engine_files:
        engine = cryproject.load(engine_file)
        engine_id = engine['info']['id']
        if engine_id in engine_registry:
            del engine_registry[engine_id]
            removed.append(engine_id)

    if removed:
        sys.exit(cryregistry.save_engines(engine_registry, register_action=False, silent=args.silent))

#--- SWITCH ---

def cryswitch_enginenames(engines):
    """
    Returns a list of the names the engines.
    """
    return list(map(lambda a: (a[1]), engines))

class CrySwitch(tk.Frame):
    """
    Handles the GUI used to switch engine version of a project.
    """
    def __init__(self, project_file, engine_list, found):
        root = tk.Tk()
        super().__init__(root)

        self.root = root
        self.project_file = project_file
        self.engine_list = engine_list

        self.layout(self.root)
        if self.engine_list:
            self.combo.current(found)

    def center_window(self):
        """
        Centers the window.
        """
        self.root.update_idletasks()
        width = self.root.winfo_width()
        height = self.root.winfo_height()
        x = (self.root.winfo_screenwidth() // 2) - (width // 2)
        y = (self.root.winfo_screenheight() // 2) - (height // 2)
        self.root.geometry('+{}+{}'.format(x, y))

    def layout(self, root):
        """
        Sets up the layout of the switch engine window.
        """
        window_width = 400
        window_height = 60
        root.title("Switch CRYENGINE version")
        root.minsize(width=window_width, height=window_height)
        root.resizable(width=False, height=False)
        self.center_window()

        iconfile = "editor_icon16.ico"
        if not hasattr(sys, "frozen"):
            iconfile = os.path.join(os.path.dirname(__file__), iconfile)
        else:
            iconfile = os.path.join(sys.prefix, iconfile)

        root.iconbitmap(iconfile)

        #---

        top = tk.Frame(root)
        top.pack(side='top', fill='both', expand=True, padx=5, pady=5)

        self.open = tk.Button(top, text="...", width=3, command=self.command_browse)
        self.open.pack(side='right', padx=2)

        self.combo = ttk.Combobox(top, values=cryswitch_enginenames(self.engine_list), state='readonly')
        self.combo.pack(fill='x', expand=True, side='right', padx=2)

        #---

        bottom = tk.Frame(root)
        bottom.pack(side='bottom', fill='both', expand=True, padx=5, pady=5)

        self.cancel = tk.Button(bottom, text='Cancel', width=10, command=self.close)
        self.cancel.pack(side='right', padx=2)

        self.ok_button = tk.Button(bottom, text='OK', width=10, command=self.command_ok)
        self.ok_button.pack(side='right', padx=2)

    def close(self):
        """
        Closes this window.
        """
        self.root.destroy()

    def command_ok(self):
        """
        Called when the Ok button is pressed.
        """
        i = self.combo.current()
        (engine_id, name) = self.engine_list[i]
        if engine_id is None:
            engine_dirname = name

            listdir = os.listdir(engine_dirname)
            engine_files = list(filter(lambda filename: os.path.isfile(os.path.join(engine_dirname, filename)) and os.path.splitext(filename)[1] == cryregistry.ENGINE_EXTENSION, listdir))
            engine_files.sort()
            if not engine_files:
                if HAS_WIN_MODULES:
                    message = 'Folder is not a previously registered CRYENGINE folder. Would you like to add the folder as a custom engine?'
                    if MESSAGEBOX(None, message, 'Switch engine version', win32con.MB_OKCANCEL | win32con.MB_ICONWARNING) == win32con.IDCANCEL:
                        return

                engine_id = "{%s}" % uuid.uuid4()
                engine_path = os.path.join(engine_dirname, os.path.basename(engine_dirname) + cryregistry.ENGINE_EXTENSION)
                file = open(engine_path, 'w')
                file.write(json.dumps({'info': {'id': engine_id}}, indent=4, sort_keys=True))
                file.close()
            else:
                engine_path = os.path.join(engine_dirname, engine_files[0])
                engine = cryproject.load(engine_path)
                engine_id = engine['info']['id']

            # When using the switch engine UI we don't need to be silent anymore.
            error_code = add_engines(engine_path, silent=False)
            if error_code != 0:
                self.close()
                return error_code

        self.close()

        return switch_engine(self.project_file, engine_id, False)

    def command_browse(self):
        """
        Called if the browse button is pressed.
        """
        #http://tkinter.unpythonic.net/wiki/tkFileDialog
        file = filedialog.askdirectory(mustexist=True)
        if file:
            self.engine_list.append((None, os.path.abspath(file)))
            self.combo['values'] = cryswitch_enginenames(self.engine_list)
            self.combo.current(len(self.engine_list) - 1)

def cmd_switch(args):
    """
    Command to switch the project to another engine. By specifying an
    engine version the project will instantly switch to that engine.
    Otherwise a GUI is shown where the user can select the engine version.
    """
    if not os.path.isfile(args.project_file):
        error_project_not_found(args)

    project = cryproject.load(args.project_file)
    if project is None:
        error_project_json_decode(args)

    #--- If the engine version is already specified than set it and return early.
    if args.engine_version != None:
        sys.exit(switch_engine(args.project_file, args.engine_version, args.silent))

    engine_list = []
    engine_version = []

    engines = cryregistry.load_engines()
    for(engine_id, engine_data) in engines.items():
        engine_file = engine_data['uri']

        info = engine_data.get('info', {})
        name = info.get('name', os.path.dirname(engine_file))
        version = info.get('version')
        if version is not None:
            engine_version.append((version, name, engine_id))
        else:
            engine_list.append((name, engine_id))

    engine_version.sort()
    engine_version.reverse()
    engine_list.sort()

    engine_list = list(map(lambda a: (a[2], a[1]), engine_version)) + list(map(lambda a: (a[1], a[0]), engine_list))

    engine_id = cryproject.engine_id(project)

    found = 0
    for i in range(len(engine_list)):
        (id_, name) = engine_list[i]
        if id_ == engine_id:
            found = i
            break

    app = CrySwitch(args.project_file, engine_list, found)
    app.mainloop()

def switch_engine(project_file, engine_id, silent):
    """
    Switch the engine of the selected project to the specified engine.
    By setting silent to True, no permissions will be asked from the user
    and warnings are not given.
    """
    project = cryproject.load(project_file)
    if cryproject.engine_id(project) != engine_id:
        if not silent and HAS_WIN_MODULES:
            message = ('Changing the version of the engine can cause the project to become unstable. '
                       'Do you want to create a backup before switching the engine?')
            result = MESSAGEBOX(None, message, 'Changing engine version', win32con.MB_YESNOCANCEL | win32con.MB_ICONWARNING)

            if result == win32con.IDCANCEL:
                return 1 # Return 1 to indicate that changing the engine is canceled by the user.

            if result == win32con.IDYES:
                create_backup(project_file)

        try:
            project['require']['engine'] = engine_id
            cryproject.save(project, project_file)
        except:
            # Catch every exception and print it to the console.
            # This way the command can be debugged in the console but the normal user will not be overwhelmed with technical stuff.
            exception_info = sys.exc_info()[0]
            print(repr(exception_info))
            message = 'An error occurred while changing the engine version. Is the project file read-only?'
            if not silent and HAS_WIN_MODULES:
                MESSAGEBOX(None, message, 'An error occurred', win32con.MB_OK | win32con.MB_ICONERROR)
            else:
                sys.stderr.write(message)
            return 1

        if not silent and HAS_WIN_MODULES:
            message = ('The engine version has changed and this has caused the code to become incompatible. '
                       'Please generate the solution, fix any errors in the code and rebuild the project before launching it.')
            MESSAGEBOX(None, message, 'Rebuild required', win32con.MB_OK | win32con.MB_ICONWARNING)

    return 0

#--- BACKUP ---

def cmd_backup(args):
    """
    Command for creating a backup of a project.
    """
    create_backup(args.project_file, args.backup_location, args.silent)

def create_backup(project_file, backup_location=None, silent=False):
    """
    Creates a backup of the project. A default location can be set with
    backup_location. If no dialog should be show the option silent can
    be set to True.
    """
    if not os.path.isfile(project_file):
        error_project_not_found(project_file)

    backup_project.run(project_file, backup_location, silent)

#--- UPGRADE ---

def cmd_upgrade(args):
    """
    Upgrades the project by calling it's engines cryrun.exe.
    """
    registry = cryregistry.load_engines()
    engine_path = cryregistry.engine_path(registry, args.engine_version)
    if engine_path is None:
        error_engine_path_not_found(args, args.engine_version)

    if getattr(sys, 'frozen', False):
        subcmd = [
            os.path.join(engine_path, 'Tools', 'CryVersionSelector', 'cryrun.exe')
        ]
    else:
        subcmd = [
            get_python_path(),
            os.path.join(engine_path, 'Tools', 'CryVersionSelector', 'cryrun.py')
        ]

    if not os.path.isfile(subcmd[-1]):
        error_engine_tool_not_found(args, subcmd[-1])

    sys_argv = [x for x in sys.argv[1:] if x not in ('--silent', )]
    subcmd.extend(sys_argv)

    print_subprocess(subcmd)
    sys.exit(subprocess.call(subcmd))

#--- RUN ----

def cmd_run_project(args, sys_argv=sys.argv[1:]):
    """
    Runs the command on the project, by invoking it on the cryrun.exe
    that is used by the engine the project is registered to.
    """
    if not os.path.isfile(args.project_file):
        error_project_not_found(args)

    project = cryproject.load(args.project_file)
    if project is None:
        error_project_json_decode(args)

    engine_id = cryproject.engine_id(project)
    engine_path = ""

    # Start by checking for the ability to specifying use of the local engine
    if engine_id is '.':
        engine_path = os.path.dirname(args.project_file)
    else:
        # Now check the registry
        engine_registry = cryregistry.load_engines()
        engine_path = cryregistry.engine_path(engine_registry, engine_id)
        if engine_path is None:
            error_engine_path_not_found(args, engine_id)

    if getattr(sys, 'frozen', False):
        subcmd = [
            os.path.join(engine_path, 'Tools', 'CryVersionSelector', 'cryrun.exe')
        ]
    else:
        subcmd = [
            get_python_path(),
            os.path.join(engine_path, 'Tools', 'CryVersionSelector', 'cryrun.py')
        ]

    if not os.path.isfile(subcmd[-1]):
        error_engine_tool_not_found(args, subcmd[-1])

    subcmd.extend(sys_argv)

    print_subprocess(subcmd)
    process = subprocess.Popen(subcmd, stderr=subprocess.PIPE)
    returncode = process.wait()

    if not args.silent and returncode != 0:
        title = command_title(args)
        text = process.stderr.read().strip().decode()
        if not text:
            text = SUBPROCESS_NO_STDERR
        if HAS_WIN_MODULES:
            result = MESSAGEBOX(None, text, title, win32con.MB_OKCANCEL | win32con.MB_ICONERROR)
            if result == win32con.IDOK:
                input() # Keeps the window from closing
        else:
            sys.stderr.write(text)

    if returncode != 0:
        sys.exit(returncode)

def cmd_run_engine(args, sys_argv=sys.argv[1:]):
    """
    Runs the command on the engine, by invoking it on the cryrun.exe
    that belongs to the engine.
    """
    if not os.path.isfile(args.engine_file):
        error_engine_not_found(args)

    engine_path = os.path.dirname(args.engine_file)

    if getattr(sys, 'frozen', False):
        subcmd = [
            os.path.join(engine_path, 'Tools', 'CryVersionSelector', 'cryrun.exe')
        ]
    else:
        subcmd = [
            get_python_path(),
            os.path.join(engine_path, 'Tools', 'CryVersionSelector', 'cryrun.py')
        ]

    if not os.path.isfile(subcmd[-1]):
        error_engine_tool_not_found(args, subcmd[-1])

    sys_argv = [x for x in sys_argv if x not in ('--silent', )]
    subcmd.extend(sys_argv)

    print_subprocess(subcmd)
    p = subprocess.Popen(subcmd, stderr=subprocess.PIPE)
    returncode = p.wait()

    if not args.silent and returncode != 0:
        title = command_title(args)
        text = p.stderr.read().strip().decode()
        if not text:
            text = SUBPROCESS_NO_STDERR
        if HAS_WIN_MODULES:
            result = MESSAGEBOX(None, text, title, win32con.MB_OKCANCEL | win32con.MB_ICONERROR)
            if result == win32con.IDOK:
                input() # Keeps the window from closing
        else:
            sys.stderr.write(text)

    if returncode != 0:
        sys.exit(returncode)

#--- MAIN ---
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--pause', action='store_true')
    parser.add_argument('--silent', action='store_true')

    subparsers = parser.add_subparsers(dest='command')
    subparsers.required = True

    subparsers.add_parser('install').set_defaults(func=cmd_install)
    subparsers.add_parser('uninstall').set_defaults(func=cmd_uninstall)

    parser_add = subparsers.add_parser('add')
    parser_add.add_argument('engine_files', nargs='+')
    parser_add.set_defaults(func=cmd_add)

    parser_remove = subparsers.add_parser('remove')
    parser_remove.add_argument('engine_files', nargs='+')
    parser_remove.set_defaults(func=cmd_remove)

    parser_switch = subparsers.add_parser('switch')
    parser_switch.add_argument('project_file')
    parser_switch.add_argument('engine_version', nargs='?')
    parser_switch.set_defaults(func=cmd_switch)

    parser_backup = subparsers.add_parser('backup')
    parser_backup.add_argument('project_file')
    parser_backup.add_argument('backup_location', nargs='?')
    parser_backup.set_defaults(func=cmd_backup)

    #---

    parser_upgrade = subparsers.add_parser('upgrade')
    parser_upgrade.add_argument('--engine_version', default='')
    parser_upgrade.add_argument('project_file')
    parser_upgrade.set_defaults(func=cmd_upgrade)

    parser_projgen = subparsers.add_parser('projgen')
    parser_projgen.add_argument('project_file')
    parser_projgen.add_argument('remainder', nargs=argparse.REMAINDER)
    parser_projgen.set_defaults(func=cmd_run_project)

    parser_projgen = subparsers.add_parser('cmake-gui')
    parser_projgen.add_argument('project_file')
    parser_projgen.add_argument('remainder', nargs=argparse.REMAINDER)
    parser_projgen.set_defaults(func=cmd_run_project)

    parser_build = subparsers.add_parser('build')
    parser_build.add_argument('project_file')
    parser_build.add_argument('remainder', nargs=argparse.REMAINDER)
    parser_build.set_defaults(func=cmd_run_project)

    parser_edit = subparsers.add_parser('edit')
    parser_edit.add_argument('project_file')
    parser_edit.add_argument('remainder', nargs=argparse.REMAINDER)
    parser_edit.set_defaults(func=cmd_run_project)

    parser_open = subparsers.add_parser('open')
    parser_open.add_argument('project_file')
    parser_open.add_argument('remainder', nargs=argparse.REMAINDER)
    parser_open.set_defaults(func=cmd_run_project)

    parser_server = subparsers.add_parser('server')
    parser_server.add_argument('project_file')
    parser_server.add_argument('remainder', nargs=argparse.REMAINDER)
    parser_server.set_defaults(func=cmd_run_project)

    parser_metagen = subparsers.add_parser('metagen')
    parser_metagen.add_argument('project_file')
    parser_metagen.add_argument('remainder', nargs=argparse.REMAINDER)
    parser_metagen.set_defaults(func=cmd_run_project)

    parser_package = subparsers.add_parser('package')
    parser_package.add_argument('project_file')
    parser_package.add_argument('remainder', nargs=argparse.REMAINDER)
    parser_package.set_defaults(func=cmd_run_project)

    parser_engine_gen = subparsers.add_parser('engine_gen')
    parser_engine_gen.add_argument('engine_file')
    parser_engine_gen.add_argument('remainder', nargs=argparse.REMAINDER)
    parser_engine_gen.set_defaults(func=cmd_run_engine)

    #---

    args = parser.parse_args()
    args.func(args)
    if args.pause:
        input('Press Enter to continue...')

if __name__ == '__main__':
    """
    Cryselect is distributed with the web launcher - there exists only one copy of the application on the system.
    It is intended to called by the web launcher and Windows file extension integration.
    Cryselect is responsible for maintaining the project registry, and forwarding engine commands to cryrun.
    """
    main()
