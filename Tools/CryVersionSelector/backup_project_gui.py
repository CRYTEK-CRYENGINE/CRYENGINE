#!/usr/bin/env python3
"""
This handles the TkInter GUI for making backups of projects.
It also handles it properly if the current machine isn't
capable of running TkInter.
"""

import sys
import os.path

HAS_TK = True
try:
    import tkinter as tk
    from tkinter import filedialog
except ImportError:
    print("Skipping importing tkinter, because it's not installed.")
    HAS_TK = False

def configure_backup(export_path):
    """
    Opens a GUI in which the user can select where the backup is saved.
    """
    # Return the default export_path if no GUI can be made.
    if not HAS_TK:
        return export_path

    iconfile = "editor_icon16.ico"
    if not hasattr(sys, "frozen"):
        iconfile = os.path.join(os.path.dirname(__file__), iconfile)
    else:
        iconfile = os.path.join(sys.prefix, iconfile)

    root = tk.Tk()
    root.iconbitmap(iconfile)
    app = CryConfigureBackup(export_path=export_path, master=root)

    app.mainloop()
    path = app.export_path

    return path

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
    class CryConfigureBackup(tk.Frame):
        """
        TK class that handles the window to configure the backup.
        """
        export_path = None

        def __init__(self, export_path=None, master=None):
            super().__init__(master)
            self.parent = master
            self.parent.title("CRYENGINE backup project")
            self.parent.minsize(400, 100)
            self.pack(expand=True, fill=tk.BOTH)
            center_window(self.parent)
            self.create_widgets(export_path)

        def create_widgets(self, export_path):
            """
            Sets up the main window.
            """
            # Export path browse dialog
            self.browse_frame = tk.Frame(self)
            self.browse_frame.pack(side='top', fill=tk.BOTH, expand=True, padx=5, pady=5)

            tk.Label(self.browse_frame, text="Backup location:").pack()

            self.browse_button = tk.Button(self.browse_frame, text="...", width=3, command=self.browse_cmd)
            self.browse_button.pack(side="right", padx=2)

            self.path_value = tk.StringVar()
            self.path_value.set(export_path)
            self.dir_box = tk.Entry(self.browse_frame, textvariable=self.path_value)
            self.dir_box.pack(fill=tk.X, expand=True, side="right", padx=2)

            # Confirm button
            self.confirm = tk.Button(self)
            self.confirm["text"] = "Confirm"
            self.confirm["command"] = self.confirm_cmd
            self.confirm.pack(padx=5, pady=5)

        def confirm_cmd(self):
            """
            Called when the select button is pressed.
            """
            self.export_path = self.path_value.get()
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
            Closes this window
            """
            self.parent.destroy()
