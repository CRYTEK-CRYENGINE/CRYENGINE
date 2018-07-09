#!python3
"""
This script makes a backup of the project files into a single directory.
It only backs up the assets, code and assemblies of the project.
Files added outside of the standard folders are not included.
"""
import sys
import os
import errno
import shutil
import stat
import fnmatch
import platform
import cryproject
import backup_project_gui

HAS_WIN_MODULES = True
try:
    import ctypes
    import win32con
    MESSAGEBOX = ctypes.windll.user32.MessageBoxW
except ImportError:
    HAS_WIN_MODULES = False

BIN_DIRS = [
    os.path.join("bin", "win_x64"),
    os.path.join("bin", "win_x64_release"),
    os.path.join("bin", "win_x86"),
    os.path.join("bin", "win_x86_release"),
    os.path.join("bin", "linux_x64_clang"),
    os.path.join("bin", "linux_x64_clang_release"),
    os.path.join("bin", "linux_x64_gcc"),
    os.path.join("bin", "linux_x64_gcc_release")
]

# "\\\\?\\" is added to make sure copying doesn't crash on Windows because the path gets too long for some files.
# This is a requirement to copy the Mono folder on Windows.
LONG_PATH_PREFIX = "\\\\?\\" if platform.system() == "Windows" else ""

def run(project_file, backup_location, silent):
    """
    Entry point for setting up the process to create a backup of a project.
    """
    # Path to the project file as created by the launcher - engine and project path are derivable from this.
    project = cryproject.load(project_file)
    project_title = project['info']['name']

    # The path the folder that contains the .cryproject file.
    project_path = os.path.normpath(os.path.dirname(project_file))
    project_path_long = LONG_PATH_PREFIX + project_path

    project_file_name = os.path.basename(project_file)

    # Path to which the game is to be exported.
    head, tail = os.path.split(project_path)
    if backup_location and is_pathname_valid(backup_location):
        export_path = backup_location
    else:
        export_path = os.path.join(head, '{}_backup'.format(tail))

    while not silent:
        export_path = backup_project_gui.configure_backup(export_path)
        if not export_path:
            # If no valid path is selected it's most likely because
            # the user closed the window, so close this as well.
            return 1

        export_path = os.path.normpath(export_path)
        temp_project_file = os.path.join(export_path, project_file_name)
        if os.path.isfile(temp_project_file) and os.path.samefile(temp_project_file, project_file):
            message = "The backup directory cannot be the same as the current project directory!"
            if HAS_WIN_MODULES:
                MESSAGEBOX(None, message, 'Invalid folder selected!', win32con.MB_OK | win32con.MB_ICONWARNING)
            else:
                print(message)
            continue

        if HAS_WIN_MODULES and os.path.isdir(export_path) and os.listdir(export_path):
            message = ('The directory "{}" is not empty. '
                       'Continuing to write the backup to this location can cause data loss!'
                       '\nAre you sure you wish to continue?'
                      ).format(export_path)
            if MESSAGEBOX(None, message, 'Overwrite existing folder', win32con.MB_OKCANCEL | win32con.MB_ICONWARNING) == win32con.IDCANCEL:
                continue
        break

    export_path_long = LONG_PATH_PREFIX + export_path

    if not silent:
        print("Creating a backup of project {}".format(project_title))
        print("Backup is saved to: {}".format(export_path))

    task_list = []

    task_list.append(("Copying code folder...", copy_code, project, project_path_long, export_path_long, silent))
    task_list.append(("Copying project binaries...", copy_binaries, project, project_path, export_path, silent))
    task_list.append(("Copying game assets...", copy_assets, project, project_path_long, export_path_long, silent))
    task_list.append(("Copying shared libraries...", copy_libs, project, project_path_long, export_path_long))
    task_list.append(("Copying config files...", copy_configs, project_path_long, export_path_long, project_file_name))

    i = 0
    count = len(task_list)
    for task in task_list:
        description = task[0]
        if not silent:
            print(description)
            set_title("{}% {}".format(int(get_percentage(i, count)), description))
        task[1](*task[2:])
        i += 1

    if not silent:
        set_title("100% Backup created successfully")
        message = "Backup created successfully"
        if HAS_WIN_MODULES:
            MESSAGEBOX(None, message, 'Backup created', win32con.MB_OK | win32con.MB_ICONINFORMATION)
        else:
            print(message)
            input("Press Enter to exit")

def set_title(title):
    """
    Sets the title of the command line window this runs in.
    """
    if not title:
        title = "Building..."

    #using the kernel32 should be better, but in case it's not working it can switch to using system().
    if HAS_WIN_MODULES:
        try:
            kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
            if kernel32:
                kernel32.SetConsoleTitleW(u"{}".format(title))
        except:
            system("title {}".format(title))
    else:
        system("title {}".format(title))

def get_percentage(index, count):
    """
    Returns index / count but as a percentage from 0 - 100.
    """
    return (100.0 / count) * index

def sanitize_for_fn(text):
    """
    Escapes the [ and ] characters by wrapping them in [].
    """
    return text.translate(str.maketrans({'[': '[[]', ']': '[]]'}))

def copy_code(project, project_path, export_path, silent):
    """
    Copies the code and solutions to the backup folder.
    """
    code_dir = cryproject.cmakelists_dir(project)
    copy_directory(project_path, export_path, code_dir, silent=silent)

    # Also try copying the solutions folder if available
    solutions_dir = "solutions"
    copy_directory(project_path, export_path, solutions_dir, warn_on_fail=False, silent=silent)

def copy_assets(project, project_path, export_path, silent):
    """
    Copies the assets of the project to the backup folder.
    """
    assets_dir = cryproject.asset_dir(project)
    copy_directory(project_path, export_path, assets_dir, silent=silent)

def copy_binaries(project, project_path, export_path, silent):
    """
    Copies the projects binaries files (.dll) and
    related files such as debug symbols.
    """
    plugins = cryproject.plugins_list(project)

    if not plugins:
        return

    for plugin in plugins:
        path = plugin.get("path")
        plugin_type = plugin.get("type")

        if not path:
            continue

        # Skip engine plugins. They're not included in the backup.
        if not path.endswith(".dll"):
            continue

        files = []

        if plugin_type == "EPluginType::Native":
            for bin_dir in BIN_DIRS:
                # Try to apply the selected engine configuration to the plugin as well.
                # This does assume plugins follow the same naming-rules as the engine does, as defined in bin_dirs.
                file_path = os.path.join(bin_dir, os.path.basename(path))
                src_file = os.path.join(project_path, file_path)
                if os.path.isfile(src_file):
                    dst_file = os.path.join(export_path, file_path)
                    files.append((src_file, dst_file))

        if not files:
            src_file = os.path.join(project_path, path)
            dst_file = os.path.join(export_path, path)
            files.append((src_file, dst_file))

        for src_file, dst_file in files:
            if os.path.isfile(src_file):
                os.makedirs(os.path.dirname(dst_file), exist_ok=True)
                shutil.copy2(src_file, dst_file)

                # Copy debug symbols
                src_file = "{}.pdb".format(os.path.splitext(src_file)[0])
                if os.path.isfile(src_file):
                    dst_file = "{}.pdb".format(os.path.splitext(dst_file)[0])
                    shutil.copy2(src_file, dst_file)
            elif not silent:
                print('Unable to copy plugin because the file "{}" doesn\'t exist!'.format(src_file))

def copy_libs(project, project_path, export_path):
    """
    Searches the bin folder for files that fit the name of the shared libs,
    and copies them to the backup directory.
    """
    libs = cryproject.libs_list(project)

    if not libs:
        return

    # The bin folders are optional since they can be generated from the source code.
    # So if they don't exist just skip them.
    bin_dir = os.path.join(project_path, "bin")
    if not os.path.isdir(bin_dir):
        return

    export_bin = os.path.join(export_path, "bin")

    exclude = ["*.ilk"]

    for lib in libs:
        if not lib:
            continue

        shared = lib.get("shared", None)
        if not shared:
            continue

        any_config = shared.get("any", None)
        win86 = shared.get("win_x86", None)
        win64 = shared.get("win_x64", None)

        if any_config:
            include = ["{}*".format(any_config)]
            copy_directory_contents(bin_dir, export_bin, include, exclude, overwrite=True)

        if win86:
            include = ["{}*".format(win86)]
            src_path = os.path.join(bin_dir, "win_x86")
            dst_path = os.path.join(export_bin, "win_x86")
            if os.path.isdir(src_path):
                copy_directory_contents(src_path, dst_path, include, exclude, overwrite=True)

        if win64:
            include = ["{}*".format(win64)]
            src_path = os.path.join(bin_dir, "win_x64")
            dst_path = os.path.join(export_bin, "win_x64")
            if os.path.isdir(src_path):
                copy_directory_contents(src_path, dst_path, include, exclude, overwrite=True)

def copy_configs(project_path, export_path, project_file_name):
    """
    Copies the cryproject file, and the .cfg files of the project.
    """
    copy_file(project_path, export_path, project_file_name)
    copy_file(project_path, export_path, "autoexec.cfg")
    copy_file(project_path, export_path, "system.cfg")
    copy_file(project_path, export_path, "editor.cfg")
    copy_file(project_path, export_path, "user.cfg")

def copy_file(project_path, export_path, file_path):
    """
    Copies a file from the project to the backup folder.
    """
    src_file = os.path.join(project_path, file_path)
    if os.path.isfile(src_file):
        dst_file = os.path.join(export_path, file_path)
        os.makedirs(os.path.dirname(dst_file), exist_ok=True)
        if os.path.isfile(dst_file):
            os.chmod(dst_file, stat.S_IWRITE)
            os.remove(dst_file)
        shutil.copy2(src_file, dst_file)

def copy_directory(project_path, export_path, copy_dir, warn_on_fail=True, silent=False):
    """
    Copies the directory from the project to the backup folder.
    """
    if not copy_dir:
        if warn_on_fail and not silent:
            print("Skipping copying because the directory name is not set.")
        return

    src_path = os.path.join(project_path, copy_dir)

    if not os.path.isdir(src_path):
        if warn_on_fail and not silent:
            print("Unable to copy {} because it doesn't exist!".format(src_path))
        return

    dst_path = os.path.join(export_path, copy_dir)

    if os.path.isdir(dst_path):
        shutil.rmtree(dst_path, onerror=on_rm_error)
    try:
        shutil.copytree(src_path, dst_path)
    # This exception is thrown when the user still has locked files in the project,
    # for example db.lock from Visual Studio.
    except shutil.Error as e:
        errors = e.args[0]
        for error in errors:
            src, dst, msg = error
            print('Unable to copy "{}" to "{}"!\nError:{}'.format(src, dst, msg))
    except IOError as e:
        print('Error: {}'.format(e.strerror))

def on_rm_error(func, path, exc_info):
    """
    Clears the read-only flag of the file at path, and unlinks it.
    :param func: The function that raised the exception.
    :param path: The path of the file that couldn't be removed.
    :param exc_info: The exception information returned by sys.exc_info().
    """
    os.chmod(path, stat.S_IWRITE)
    os.remove(path)

def copy_directory_contents(src_dir, dst_dir, include_patterns=None, exclude_patterns=None, recursive=True, overwrite=False):
    """
    Copies all files that match the include patterns and don't
    match the exclude patterns to the destination directory.
    If the include_patterns are empty every file will be included.
    If the exclude_patterns are emtpty no file will be excluded.
    """
    clean_dir = sanitize_for_fn(src_dir)
    for file in os.listdir(src_dir):
        src_path = os.path.join(src_dir, file)
        dst_path = os.path.join(dst_dir, file)

        if os.path.isdir(src_path) and recursive:
            copy_directory_contents(src_path, dst_path, include_patterns, exclude_patterns, recursive, overwrite)
            continue

        if exclude_patterns:
            exclude = False
            for pattern in exclude_patterns:
                exclude = fnmatch.fnmatch(src_path, os.path.join(clean_dir, pattern))
                if exclude:
                    break
            if exclude:
                continue

        if include_patterns:
            include = False
            for pattern in include_patterns:
                include = fnmatch.fnmatch(src_path, os.path.join(clean_dir, pattern))
                if include:
                    break
            if not include:
                continue

        if os.path.isfile(dst_path) and not overwrite:
            continue

        os.makedirs(dst_dir, exist_ok=True)

        shutil.copy2(src_path, dst_path)

# Path validation retrieved from https://stackoverflow.com/questions/9532499/check-whether-a-path-is-valid-in-python-without-creating-a-file-at-the-paths-ta
# Sadly, Python fails to provide the following magic number for us.
ERROR_INVALID_NAME = 123
"""
Windows-specific error code indicating an invalid pathname.

See Also
----------
https://msdn.microsoft.com/en-us/library/windows/desktop/ms681382%28v=vs.85%29.aspx
    Official listing of all such codes.
"""

def is_pathname_valid(pathname: str) -> bool:
    """
    `True` if the passed pathname is a valid pathname for the current OS;
    `False` otherwise.
    """
    # If this pathname is either not a string or is but is empty, this pathname
    # is invalid.
    try:
        if not isinstance(pathname, str) or not pathname:
            return False

        # Strip this pathname's Windows-specific drive specifier (e.g., `C:\`)
        # if any. Since Windows prohibits path components from containing `:`
        # characters, failing to strip this `:`-suffixed prefix would
        # erroneously invalidate all valid absolute Windows pathnames.
        _, pathname = os.path.splitdrive(pathname)

        # Directory guaranteed to exist. If the current OS is Windows, this is
        # the drive to which Windows was installed (e.g., the "%HOMEDRIVE%"
        # environment variable); else, the typical root directory.
        root_dirname = os.environ.get('HOMEDRIVE', 'C:') \
            if sys.platform == 'win32' else os.path.sep
        assert os.path.isdir(root_dirname)   # ...Murphy and her ironclad Law

        # Append a path separator to this directory if needed.
        root_dirname = root_dirname.rstrip(os.path.sep) + os.path.sep

        # Test whether each path component split from this pathname is valid or
        # not, ignoring non-existent and non-readable path components.
        for pathname_part in pathname.split(os.path.sep):
            try:
                os.lstat(root_dirname + pathname_part)
            # If an OS-specific exception is raised, its error code
            # indicates whether this pathname is valid or not. Unless this
            # is the case, this exception implies an ignorable kernel or
            # filesystem complaint (e.g., path not found or inaccessible).
            #
            # Only the following exceptions indicate invalid pathnames:
            #
            # * Instances of the Windows-specific "WindowsError" class
            #   defining the "winerror" attribute whose value is
            #   "ERROR_INVALID_NAME". Under Windows, "winerror" is more
            #   fine-grained and hence useful than the generic "errno"
            #   attribute. When a too-long pathname is passed, for example,
            #   "errno" is "ENOENT" (i.e., no such file or directory) rather
            #   than "ENAMETOOLONG" (i.e., file name too long).
            # * Instances of the cross-platform "OSError" class defining the
            #   generic "errno" attribute whose value is either:
            #   * Under most POSIX-compatible OSes, "ENAMETOOLONG".
            #   * Under some edge-case OSes (e.g., SunOS, *BSD), "ERANGE".
            except OSError as exc:
                if hasattr(exc, 'winerror'):
                    if exc.winerror == ERROR_INVALID_NAME:
                        return False
                elif exc.errno in {errno.ENAMETOOLONG, errno.ERANGE}:
                    return False
    # If a "TypeError" exception was raised, it almost certainly has the
    # error message "embedded NUL character" indicating an invalid pathname.
    except TypeError as exc:
        return False
    # If no exception was raised, all path components and hence this
    # pathname itself are valid. (Praise be to the curmudgeonly python.)
    else:
        return True
    # If any other exception was raised, this is an unrelated fatal issue
    # (e.g., a bug). Permit this exception to unwind the call stack.
    #
    # Did we mention this should be shipped with Python already?
