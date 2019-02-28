#!python3
"""
This script combines the required engine and project files into a single directory.
It also creates .pak files from the asset directory and writes an appropriate system.cfg.
"""
import os
import sys
import shutil
import stat
import fnmatch
import subprocess
import platform
import cryproject
import release_project_gui

HAS_WIN_MODULES = True
try:
    import ctypes
except ImportError:
    HAS_WIN_MODULES = False

DEFAULT_CONFIGURATIONS = [
    ("win_x64:Profile", os.path.join("bin", "win_x64")),
    ("win_x64:Release", os.path.join("bin", "win_x64_release")),
    ("win_x86:Profile", os.path.join("bin", "win_x86")),
    ("win_x86:Release", os.path.join("bin", "win_x86_release")),
    ("linux_x64_clang:Profile", os.path.join("bin", "linux_x64_clang")),
    ("linux_x64_clang:Release", os.path.join("bin", "linux_x64_clang_release")),
    ("linux_x64_gcc:Profile", os.path.join("bin", "linux_x64_gcc")),
    ("linux_x64_gcc:Release", os.path.join("bin", "linux_x64_gcc_release"))
]

CONFIGURATION_PLATFORM_LOOKUP = {
    "win_x64:Profile": "Windows",
    "win_x64:Release": "Windows",
    "win_x86:Profile": "Windows",
    "win_x86:Release": "Windows",
    "linux_x64_clang:Profile": "Linux",
    "linux_x64_clang:Release": "Linux",
    "linux_x64_gcc:Profile": "Linux",
    "linux_x64_gcc:Release": "Linux",
}

# "\\\\?\\" is added to make sure copying doesn't crash on Windows because the path gets too long for some files.
# This is a requirement to copy the Mono folder on Windows.
LONG_PATH_PREFIX = "\\\\?\\" if platform.system() == "Windows" else ""

def run(project_file):
    """
    Entry point for setting up the process to release a packaged build.
    """
    # Path to the project file as created by the launcher - engine and project path are derivable from this.
    project = cryproject.load(project_file)
    project_title = project['info']['name']

    # The path the folder that contains the .cryproject file.
    project_path = os.path.normpath(os.path.dirname(project_file))
    project_path_long = LONG_PATH_PREFIX + project_path

    # The path to the engine that is being used by the project.
    engine_path = os.path.normpath(get_engine_path())
    engine_path_long = LONG_PATH_PREFIX + engine_path

    # Path to which the game is to be exported.
    export_path = os.path.join(project_path, '{}_package'.format(project_title))

    configurations = get_available_configurations(engine_path)
    if not configurations:
        print("Unable to find a valid engine configuration. Make sure to build your engine to the following locations.")
        for key, value in DEFAULT_CONFIGURATIONS:
            print("Configuration: {} \n Location: {}".format(key, value))
        print("Press Enter to exit")
        input()
        return

    # configuration is returned as (export_path, config_name, config_bin_folder, include_symbols)
    configuration = release_project_gui.configure_build(export_path, configurations)
    if not configuration:
        # No configuration selected.
        # Most likely because the user closed the window, so close this as well.
        return

    export_path = os.path.normpath(configuration[0])
    export_path_long = LONG_PATH_PREFIX + export_path
    config_type = configuration[1]
    bin_path = os.path.normpath(configuration[2])
    include_symbols = configuration[3]

    print("Packaging project {}".format(project_title))
    print("Configuration: {}".format(config_type))
    print("Debug symbols are {}".format("included" if include_symbols else "excluded"))
    print("Building to: {}".format(export_path))

    task_list = []

    if os.path.exists(export_path_long):
        task_list.append(("Deleting previous build...", delete_previous_build, export_path_long))

    task_list.append(("Packaging custom engine assets...", package_engine_assets, engine_path, export_path))
    task_list.append(("Copying default engine assets...", copy_engine_assets, engine_path_long, export_path_long))
    task_list.append(("Copying engine binaries...", copy_engine_binaries, engine_path_long, export_path_long, bin_path, include_symbols))

    if requires_mono(project, project_path_long):
        task_list.append(("Copying mono files...", copy_mono_files, engine_path_long, export_path_long))

    task_list.append(("Copying game binaries...", copy_project_plugins, project, project_path, export_path, bin_path, config_type, include_symbols))
    task_list.append(("Copying shared libraries...", copy_libs, project, project_path, export_path, include_symbols))
    task_list.append(("Copying existing game asset packages...", copy_assets, project, project_path_long, export_path_long))
    task_list.append(("Packaging game assets...", package_assets, project, engine_path, project_path, export_path))
    task_list.append(("Cleaning up temp folders...", delete_temp_folders, engine_path_long, project_path_long))
    task_list.append(("Creating config files...", create_config, project_file, export_path))

    i = 0
    count = len(task_list)
    for task in task_list:
        description = task[0]
        print(description)
        set_title("{}% {}".format(int(get_percentage(i, count)), description))
        task[1](*task[2:])
        i += 1

    set_title("100% Build packaged successfully")
    print("Build packaged successfully")
    print("Press Enter to exit")
    input()

def get_available_configurations(engine_path):
    """
    Returns the configurations that are available for this engine.
    """
    available_configurations = []
    for key, value in DEFAULT_CONFIGURATIONS:
        win_path = os.path.join(engine_path, value, "GameLauncher.exe")
        linux_path = os.path.join(engine_path, value, "GameLauncher")
        if os.path.isfile(win_path) or os.path.isfile(linux_path):
            available_configurations.append((key, value))

    return available_configurations

def delete_previous_build(export_path):
    """
    Deletes the folder found at export_path, and properly handles deleting
    read-only files.
    """
    if os.path.exists(export_path):
        shutil.rmtree(export_path, onerror=on_rm_error)

def delete_temp_folders(engine_path, project_path):
    """
    Deletes the temporary folders created by RC.
    """
    delete_temp_engine_folder(engine_path)
    delete_temp_assets_folder(project_path)

def delete_temp_engine_folder(engine_path):
    """
    Deletes the temporary engine folder created by RC.
    """
    temp_dir = os.path.abspath(os.path.join(engine_path, os.pardir, "_rc_PC"))
    if os.path.exists(temp_dir):
        os.chmod(temp_dir, stat.S_IWRITE)
        shutil.rmtree(temp_dir, onerror=on_rm_error)

def delete_temp_assets_folder(project_path):
    """
    Deletes the temporary assets folder created by RC.
    """
    temp_dir = os.path.abspath(os.path.join(project_path, os.pardir, "_rc_PC_pure"))
    if os.path.exists(temp_dir):
        os.chmod(temp_dir, stat.S_IWRITE)
        shutil.rmtree(temp_dir, onerror=on_rm_error)

def on_rm_error(func, path, exc_info):
    """
    Clears the read-only flag of the file at path, and unlinks it.
    :param func: The function that raised the exception.
    :param path: The path of the file that couldn't be removed.
    :param exc_info: The exception information returned by sys.exc_info().
    """
    os.chmod(path, stat.S_IWRITE)
    os.unlink(path)

def requires_mono(project, project_path):
    """
    Returns true if the project requires Mono, false otherwise.
    """
    if cryproject.is_managed(project):
        return True

    asset_path = os.path.join(project_path, cryproject.asset_dir(project))
    return directory_contains_file(asset_path, ["*.cs"])

def get_tools_path():
    """
    Returns the path to the Tools folder, based on the location of this file.
    """
    if getattr(sys, 'frozen', False):
        script_path = sys.executable
    else:
        script_path = __file__

    return os.path.abspath(os.path.join(os.path.dirname(script_path), '..'))

def get_engine_path():
    """
    Returns the path to the engine's root folder.
    """
    return os.path.abspath(os.path.join(get_tools_path(), '..'))

def get_percentage(index, count):
    """
    Returns index / count but as a percentage from 0 - 100.
    """
    return (100.0 / count) * index

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

def copy_engine_binaries(engine_path, export_path, rel_dir, include_symbols):
    """
    Copy a directory to its corresponding location in the export directory.
    :param engine_path: Current location of the files (project_path or engine_path).
    :param export_path: Path to which the binaries should be exported.
    :param rel_dir: Path of the directory to copy, relative to *source_dir*.
    """
    copypaths = []

    excludes = ['imageformats**',
                'ToolkitPro*',
                'platforms**',
                'Qt*',
                'mfc*',
                'CryGameSDK*',
                'CryEngine.*.dll*',
                'Sandbox*',
                'ShaderCacheGen*',
                'icu*',
                'python27*',
                'LuaCompiler*',
                'Editor**',
                'PySide2*',
                'shiboken*',
                'crashrpt*',
                'CrashSender*',
                '*.mdb',
                '*.ilk',
                '*.exp',
                '*.lib',
                '*.xml',
                '*.ipdb',
                '*.iobj',
                'CryQt*',
                '_CryQt*',
                '*.pyd'
               ]

    if not include_symbols:
        excludes.append('*.pdb')

    pwd = os.getcwd()
    os.chdir(engine_path)
    for root, _, filenames in os.walk(rel_dir):
        for filename in filenames:
            copypaths.append(os.path.normpath(os.path.join(root, filename)))
    os.chdir(pwd)

    clean_dir = sanitize_for_fn(rel_dir)
    for path in copypaths:
        excluded = False
        for pattern in excludes:
            excluded = fnmatch.fnmatch(path, os.path.join(clean_dir, pattern))
            if excluded:
                break
        if excluded:
            continue
        destpath = os.path.normpath(os.path.join(export_path, path))
        os.makedirs(os.path.dirname(destpath), exist_ok=True)
        shutil.copy(os.path.join(engine_path, path), destpath)


def copy_mono_files(engine_path, export_path):
    """
    Copy mono directory and CRYENGINE C# libraries to export path.
    """
    input_bindir = os.path.join(engine_path, 'bin')
    output_bindir = os.path.join(export_path, 'bin')

    shutil.copytree(os.path.join(input_bindir, 'common'), os.path.join(output_bindir, 'common'))

    for csharp_file in os.listdir(os.path.join(input_bindir, 'win_x64')):
        # We've already copied the non-C# libraries, so skip them here.
        if not fnmatch.fnmatch(csharp_file, 'CryEngine.*.dll'):
            continue
        shutil.copyfile(os.path.join(input_bindir, 'win_x64', csharp_file),
                        os.path.join(output_bindir, 'win_x64', csharp_file))

def run_command(command, silent=True):
    """
    Runs a command on the commandline.
    If silent is true, any output will not be printed unless an error occurs.
    """
    # Filter empty commands and convert the command to a string.
    command = list(filter(bool, command))
    command_str = ("".join("{} ".format(e) for e in command)).strip()

    # Using check_output here makes the command execute silently,
    # while still having the option to print the output in case of an error.
    try:
        if silent:
            subprocess.check_output(command_str, universal_newlines=True)
        else:
            subprocess.check_call(command_str)
    except subprocess.CalledProcessError as exception:
        if not exception.returncode == 0:
            print("Encountered and error while running command '{}'!".format(command_str))
            print(exception.output)
            return False

    return True

def package_engine_assets(engine_path, export_path):
    """
    Runs the Resource Compiler on the engine assets following the
    instructions of rcjob_release_engine_assets.xml. Outputs the
    .pak files in the Engine folder of the build.
    """

    rc_path = os.path.join(engine_path, "Tools", "rc", "rc.exe")
    rc_job_path = os.path.join(engine_path, "Tools", "CryVersionSelector", "rc_jobs", "rcjob_release_engine_assets.xml")

    if os.path.isfile(rc_path) and os.path.isfile(rc_job_path):
        pwd = os.getcwd()
        os.chdir(engine_path)
        rc_cmd = ['"{}"'.format(rc_path),
                  '/job="{}"'.format(rc_job_path),
                  '/p=PC',
                  '/pak_root="{}"'.format(export_path),
                  '/IncludeShaderSource=1',
                  '/stripMetadata',
                  '/skipmissing']
        run_command(rc_cmd)
        os.chdir(pwd)
    else:
        if not os.path.isfile(rc_path):
            print("Unable to package the engine assets with the resource compiler because '{}' is missing!".format(rc_path))
        else:
            print("Unable to package the engine assets with the resource compiler because '{}' is missing!".format(rc_job_path))

def copy_engine_assets(engine_path, export_path):
    """
    Copy the engine assets, making sure to avoid .cryasset.pak files.
    If the engine assets have already been packaged this function will
    not overwrite them.
    """
    srcpath = os.path.join(engine_path, 'engine')
    dstpath = os.path.join(export_path, 'engine')

    copy_directory_contents(srcpath, dstpath, ["*.pak"], ["*.cryasset.pak"])

def copy_assets(project, project_path, export_path):
    """
    Copies existing .pak files from the assets directory to the
    export location. Other assets can be added to these pak files
    later on in package_assets().
    """
    asset_dir = cryproject.asset_dir(project)
    srcpath = os.path.join(project_path, asset_dir)
    dstpath = os.path.join(export_path, asset_dir)

    copy_directory_contents(srcpath, dstpath, ["*.pak"], ["*.cryasset.pak"])

def copy_directory_contents(src_dir, dst_dir, include_patterns=None, exclude_patterns=None, recursive=True, overwrite=False):
    """
    Copies all files that match the include patterns and don't
    match the exclude patterns to the destination directory.
    If the include_patterns are empty every file will be included.
    If the exclude_patterns are emtpty no file will be excluded.
    """
    clean_dir = sanitize_for_fn(src_dir)
    for file in os.listdir(src_dir):
        srcpath = os.path.join(src_dir, file)
        dstpath = os.path.join(dst_dir, file)

        if os.path.isdir(srcpath) and recursive:
            copy_directory_contents(srcpath, dstpath, include_patterns, exclude_patterns, recursive, overwrite)
            continue

        if exclude_patterns:
            exclude = False
            for pattern in exclude_patterns:
                exclude = fnmatch.fnmatch(srcpath, os.path.join(clean_dir, pattern))
                if exclude:
                    break
            if exclude:
                continue

        if include_patterns:
            include = False
            for pattern in include_patterns:
                include = fnmatch.fnmatch(srcpath, os.path.join(clean_dir, pattern))
                if include:
                    break
            if not include:
                continue

        if os.path.isfile(dstpath) and not overwrite:
            continue

        os.makedirs(dst_dir, exist_ok=True)

        shutil.copyfile(srcpath, dstpath)

def directory_contains_file(directory, include_patterns, exclude_patterns=None, recursive=True):
    """
    Checks if a directory contains a file that matches the specified pattern.
    """
    clean_dir = sanitize_for_fn(directory)
    for file in os.listdir(directory):
        file_path = os.path.join(directory, file)

        if os.path.isdir(file_path) and recursive:
            if directory_contains_file(file_path, include_patterns, exclude_patterns, recursive):
                return True
            continue

        if exclude_patterns:
            exclude = False
            for pattern in exclude_patterns:
                exclude = fnmatch.fnmatch(file_path, os.path.join(clean_dir, pattern))
                if exclude:
                    break
            if exclude:
                continue

        if include_patterns:
            include = False
            for pattern in include_patterns:
                include = fnmatch.fnmatch(file_path, os.path.join(clean_dir, pattern))
                if include:
                    break
            if include:
                return True

    return False

def sanitize_for_fn(text):
    """
    Escapes the [ and ] characters by wrapping them in [].
    """
    return text.translate(str.maketrans({'[': '[[]', ']': '[]]'}))

def package_assets(project, engine_path, project_path, export_path):
    """
    Runs the Resource Compiler on the game assets following the
    instructions of rcjob_release_game_assets.xml. Outputs the
    .pak files in the Assets folder of the build that's specified
    in the project file.
    """
    rc_path = os.path.join(engine_path, "Tools", "rc", "rc.exe")
    rc_job_path = os.path.join(engine_path, "Tools", "CryVersionSelector", "rc_jobs", "rcjob_release_game_assets.xml")

    if os.path.isfile(rc_path) and os.path.isfile(rc_job_path):
        asset_dir = cryproject.asset_dir(project)
        pwd = os.getcwd()
        os.chdir(project_path)
        rc_cmd = ['"{}"'.format(rc_path),
                  '/job="{}"'.format(rc_job_path),
                  '/p=PC',
                  '/pak_root="{}"'.format(export_path),
                  '/TargetHasEditor=0',
                  '/game="{}"'.format(asset_dir),
                  '/stripMetadata',
                  '/skipmissing']
        run_command(rc_cmd)
        os.chdir(pwd)
    else:
        if not os.path.isfile(rc_path):
            print("Unable to package the game assets with the resource compiler because '{}' is missing!".format(rc_path))
        else:
            print("Unable to package the game assets with the resource compiler because '{}' is missing!".format(rc_job_path))

def create_config(project_file, export_path):
    """
    Create the config file which contains the cryproject file, and generates the system.cfg.
    """
    project_name = "game.cryproject"
    alt_project_name = "game.crygame"
    dst_file = os.path.join(export_path, project_name)
    shutil.copyfile(project_file, dst_file)
    use_config = True
    # If possible put the project file in a pak. Otherwise rename the project file's extension to crygame
    # so it won't show all the cryproject options on right-click.
    if create_config_pak(export_path, dst_file) and os.path.isfile(dst_file):
        os.remove(dst_file)
    else:
        os.rename(dst_file, os.path.join(export_path, alt_project_name))
        use_config = False

    with open(os.path.join(export_path, 'system.cfg'), 'w') as file_handle:
        if not use_config:
            file_handle.write('sys_project={}\n'.format(alt_project_name))
        file_handle.write('sys_asserts=0\n')
        file_handle.write('sys_float_exceptions = 0\n')

def create_config_pak(export_path, project_file):
    """
    Tries to package the cryproject file in a config.pak.
    Returns True if successful, False otherwise.
    """
    zip_exe = os.path.join(get_tools_path(), '7za.exe')
    if not os.path.isfile(zip_exe):
        return False

    dst_file = os.path.join(export_path, "config.pak")
    # By setting the current working directory we prevent the project file to be in a folder in the config.pak.
    pwd = os.getcwd()
    os.chdir(export_path)
    zip_cmd = ['"{}"'.format(zip_exe),
               'a',
               '-tzip',
               '"{}"'.format(dst_file),
               '"{}"'.format(project_file),
               '-mx0']
    run_command(zip_cmd)
    os.chdir(pwd)

    return os.path.isfile(dst_file)

def copy_project_plugins(project, project_path, export_path, config_path, config_type, include_symbols):
    """
    Copies all .dll files that belong to the plugins of the project.
    """
    plugins = cryproject.plugins_list(project)
    for plugin in plugins:
        path = plugin.get("path")
        plugin_type = plugin.get("type")
        platforms = plugin.get("platforms")

        if not path:
            continue

        # Skip engine plugins. They are copied elsewhere.
        if not path.endswith(".dll"):
            continue

        if platforms:
            current_platform = CONFIGURATION_PLATFORM_LOOKUP[config_type]
            platform_included = current_platform in platforms
            if not platform_included:
                continue

        src_file = os.path.join(project_path, path)
        dst_file = os.path.join(export_path, path)

        if plugin_type == "EPluginType::Native":
            # Try to apply the selected engine configuration to the plugin as well.
            # This prevents mixing release with profile configurations for example.
            # This does assume plugins follow the same naming-rules as the engine does, as defined in DEFAULT_CONFIGURATIONS.
            adjusted_src_file = os.path.join(project_path, config_path, os.path.basename(path))
            if os.path.isfile(adjusted_src_file):
                src_file = adjusted_src_file

        if os.path.isfile(src_file):
            os.makedirs(os.path.dirname(dst_file), exist_ok=True)
            shutil.copyfile(src_file, dst_file)

            # Copy debug symbols
            if include_symbols:
                src_file = "{}.pdb".format(os.path.splitext(src_file)[0])
                if os.path.isfile(src_file):
                    dst_file = "{}.pdb".format(os.path.splitext(dst_file)[0])
                    shutil.copyfile(src_file, dst_file)
        else:
            print("Failed to copy plugin file '{}' from {}!".format(path, src_file))

    # Also copy the assembly generated from C# assets
    asset_assembly = os.path.join("bin", "CRYENGINE.CSharp.dll")
    src_file = os.path.join(project_path, asset_assembly)
    if os.path.isfile(src_file):
        dst_file = os.path.join(export_path, asset_assembly)
        os.makedirs(os.path.dirname(dst_file), exist_ok=True)
        shutil.copyfile(src_file, dst_file)

        if include_symbols:
            src_file = "{}.pdb".format(os.path.splitext(src_file)[0])
            if os.path.isfile(src_file):
                dst_file = "{}.pdb".format(os.path.splitext(dst_file)[0])
                shutil.copyfile(src_file, dst_file)
            else:
                src_file = "{}.dll.mdb".format(os.path.splitext(src_file)[0])
                if os.path.isfile(src_file):
                    dst_file = "{}.dll.mdb".format(os.path.splitext(dst_file)[0])
                    shutil.copyfile(src_file, dst_file)


def copy_libs(project, project_path, export_path, include_symbols):
    """
    Searches the bin folder for files that fit the name of the shared libs,
    and copies them to the export directory.
    """
    libs = cryproject.libs_list(project)

    if not libs:
        return

    bin_dir = os.path.join(project_path, "bin")
    if not os.path.isdir(bin_dir):
        return

    export_bin = os.path.join(export_path, "bin")

    exclude = ["*.mdb", "*.xml", "*.ilk"]
    if not include_symbols:
        exclude.append("*.pdb")

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
            copy_directory_contents(bin_dir, export_bin, include, exclude)

        if win86:
            include = ["{}*".format(win86)]
            copy_directory_contents(os.path.join(bin_dir, "win_x86"), os.path.join(export_bin, "win_x86"), include, exclude)

        if win64:
            include = ["{}*".format(win64)]
            copy_directory_contents(os.path.join(bin_dir, "win_x64"), os.path.join(export_bin, "win_x64"), include, exclude)
