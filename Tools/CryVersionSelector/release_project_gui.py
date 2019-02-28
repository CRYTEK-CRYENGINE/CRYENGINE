#!/usr/bin/env python3
"""
This handles the TkInter GUI for releasing projects.
It also handles it properly if the current machine isn't
capable of running TkInter.
"""

import sys
import os.path

HAS_TK = True
try:
    import tkinter as tk
    from tkinter import ttk
    from tkinter import filedialog
except ImportError:
    print("Skipping importing tkinter, because it's not installed.")
    HAS_TK = False

DEFAULT_CONFIGURATION_FOLDER = os.path.join("bin", "win_x64")

def configure_build(export_path, configurations):
    """
    Opens a GUI in which the user can select an engine configuration to package.
    """

    if not HAS_TK:
        # Return the first configuration available, which is most likely Profile.
        if not configurations:
            return None
        return (export_path, configurations[0][0], configurations[0][1], False)

    iconfile = "editor_icon16.ico"
    if not hasattr(sys, "frozen"):
        iconfile = os.path.join(os.path.dirname(__file__), iconfile)
    else:
        iconfile = os.path.join(sys.prefix, iconfile)

    root = tk.Tk()
    root.iconbitmap(iconfile)
    app = CryConfigurationSelection(export_path=export_path, configurations=configurations, master=root)

    app.mainloop()
    config = app.selected_config
    path = app.export_path
    include_symbols = app.include_symbols

    # If the user closed the window instead of confirming the config.
    if not config:
        return None

    return (path, config[0], config[1], include_symbols)

def center_window(win):
    """
    Centers the window.
    """
    win.update_idletasks()
    width = win.winfo_width()
    height = win.winfo_height()
    position_x = (win.winfo_screenwidth() // 2) - (width // 2)
    position_y = (win.winfo_screenheight() // 2) - (height // 2)
    win.geometry('{}x{}+{}+{}'.format(width, height, position_x, position_y))

if HAS_TK:
    class CryConfigurationSelection(tk.Frame):
        """
        TK class that handles the window to select a configuration to build.
        """
        selected_config = None
        export_path = None
        include_symbols = False

        def __init__(self, export_path=None, configurations=None, master=None):
            super().__init__(master)
            self.parent = master
            self.parent.title("CRYENGINE Project packager")
            self.parent.minsize(400, 200)
            self.pack(expand=True, fill=tk.BOTH)
            self.export_path = export_path
            self.configurations = configurations
            center_window(self.parent)
            self.create_widgets()

        def create_widgets(self):
            """
            Sets up the main window.
            """
            # Export path browse dialog
            self.browse_frame = tk.Frame(self)
            self.browse_frame.pack(side='top', fill='both', expand=True, padx=5, pady=5)

            tk.Label(self.browse_frame, text="Package location:").pack()

            self.browse_button = tk.Button(self.browse_frame, text="...", width=3, command=self.browse_cmd)
            self.browse_button.pack(side="right", padx=2)

            self.path_value = tk.StringVar()
            self.path_value.set(self.export_path)
            self.dir_box = tk.Entry(self.browse_frame, textvariable=self.path_value)
            self.dir_box.pack(fill="x", expand=True, side="right", padx=2)

            # Configuration selection
            self.config_frame = tk.Frame(self)
            self.config_frame.pack(side='top', fill='both', expand=True, padx=5, pady=5)

            tk.Label(self.config_frame, text="Configuration:").pack()
            self.box_value = tk.StringVar()
            self.configs_box = ttk.Combobox(self.config_frame, textvariable=self.box_value, width=40)

            if not self.configurations:
                # If no configurations are available this window should never be created,
                # but just in case of a mistakes it's caught here.
                print("Unable to find valid configurations. "
                      "Make sure to compile the engine before packaging!")
                self.exit()
                return

            config_list = []
            for configuration in self.configurations:
                config_list.append("{} ({})".format(configuration[0], configuration[1]))
            self.configs_box['values'] = config_list

            config_index = 0
            self.configs_box.current(config_index)
            self.configs_box.pack(fill="x", expand=True, side="right", padx=2)

            # Include debug symbols option
            self.symbols_frame = tk.Frame(self)
            self.symbols_frame.pack(side='top', fill='both', expand=True, padx=5, pady=5)

            self.symbols_value = tk.BooleanVar()
            self.symbols_value.set(False)
            self.symbols_checkbox = tk.Checkbutton(self.symbols_frame, text="Include debug symbols", variable=self.symbols_value)
            self.symbols_checkbox.pack()

            # Package button
            self.confirm = tk.Button(self)
            self.confirm["text"] = "Package"
            self.confirm["command"] = self.confirm_cmd
            self.confirm.pack(padx=5, pady=5)

        def confirm_cmd(self):
            """
            Called when the select button is pressed.
            """
            current = self.configs_box.current()
            config = self.configurations[current]
            self.selected_config = config
            self.export_path = self.path_value.get()
            self.include_symbols = self.symbols_value.get()
            self.exit()

        def browse_cmd(self):
            """
            Called when the browse button is pressed.
            """
            folder = filedialog.askdirectory(mustexist=False)
            if folder:
                self.path_value.set(folder)

        def exit(self):
            """
            Called to close this window
            """
            self.parent.destroy()
