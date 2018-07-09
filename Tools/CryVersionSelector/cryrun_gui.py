#!/usr/bin/env python3

import sys
import os.path

import crysettings

HAS_WIN_MODULES = True
try:
    import winreg
except ImportError:
    HAS_WIN_MODULES = False

HAS_TK = True
try:
    import tkinter as tk
    from tkinter import ttk
except ImportError:
    print("Skipping importing tkinter, because it's not installed.")
    HAS_TK = False

def select_config(configs):
    """
    Opens a GUI in which the user can select a CMake configuration.
    Returns the selected config, or None if no selection was made.
    """

    if not HAS_TK:
        return None

    iconfile = "editor_icon16.ico"
    if not hasattr(sys, "frozen"):
        iconfile = os.path.join(os.path.dirname(__file__), iconfile)
    else:
        iconfile = os.path.join(sys.prefix, iconfile)

    root = tk.Tk()
    root.iconbitmap(iconfile)
    app = CryProjgen(configurations=configs, master=root)

    center_window(root)
    app.mainloop()
    return app.selected_config

def center_window(win):
    win.update_idletasks()
    width = win.winfo_width()
    height = win.winfo_height()
    position_x = (win.winfo_screenwidth() // 2) - (width // 2)
    position_y = (win.winfo_screenheight() // 2) - (height // 2)
    win.geometry('{}x{}+{}+{}'.format(width, height, position_x, position_y))

if HAS_TK:
    class CryProjgen(tk.Frame):
        selected_config = None

        def __init__(self, configurations=None, master=None):
            super().__init__(master)
            self.parent = master
            self.parent.title("CRYENGINE CMake Project Generator")
            self.parent.minsize(300, 100)
            self.pack()
            self.configurations = configurations
            self.settings = crysettings.Settings()
            self.create_widgets()

        def create_widgets(self):
            tk.Label(self, text="Generate Configuration: ").pack()

            self.newselection = ''
            self.box_value = tk.StringVar()
            self.configs_box = ttk.Combobox(self, textvariable=self.box_value, width=40)

            self.filtered_configs = self.filter_configs()
            if not self.filtered_configs:
                print("Unable to find Visual Studio 2015 or Visual Studio 2017. "
                      "Make sure either Visual Studio 2015 or Visual Studio 2017 is installed!")
                self.filtered_configs = self.configurations
            config_list = []
            for config in self.filtered_configs:
                config_list.append(config['title'])
            self.configs_box['values'] = config_list

            config_index = self.get_last_config_index(self.filtered_configs)
            self.configs_box.current(config_index)
            self.configs_box.pack()

            self.generate = tk.Button(self)
            self.generate["text"] = "Generate Solution"
            self.generate["command"] = self.generate_cmd
            self.generate.pack()

        def generate_cmd(self):
            current = self.configs_box.current()
            config = self.filtered_configs[current]
            self.selected_config = config
            self.settings.set_last_cmake_config(config["cmake_generator"])
            self.parent.destroy()

        def get_last_config_index(self, configs):
            last_config = self.settings.get_last_cmake_config()
            if not last_config:
                return 0
            index = 0
            for config in configs:
                if config["cmake_generator"] == last_config:
                    return index
                index += 1
            return 0

        def filter_configs(self):
            configs = []
            for config in self.configurations:
                # If it's not possible to check the registry, just add all options to the list and let the user decide.
                if not HAS_WIN_MODULES:
                    configs.append(config)
                    continue

                try:
                    registry = winreg.ConnectRegistry(None, config['compiler']['reg_key'])
                    key = winreg.OpenKey(registry, config['compiler']['key_path'])
                    if key:
                        configs.append(config)
                except:
                    # The key probably doesn't exsist, so continue
                    continue
            return configs
