#!/usr/bin/env python3

import sys
import argparse
import os.path
import subprocess
import shutil
import glob

import configparser
import tempfile
import datetime
import zipfile
import stat

import admin
import distutils.dir_util
import distutils.file_util

import cryproject
import cryregistry
import crysolutiongenerator
import release_project
import cryrun_gui

HAS_WIN_MODULES = True
try:
    import winreg
except ImportError:
    HAS_WIN_MODULES = False

#--- errors
def error_engine_not_found(path):
    sys.stderr.write("'%s' not found.\n" % path)
    sys.exit(600)

def error_project_not_found(path):
    sys.stderr.write("'%s' not found.\n" % path)
    sys.exit(600)

def error_project_json_decode(path):
    sys.stderr.write("Unable to parse '%s'.\n" % path)
    sys.exit(601)

def error_missing_engine_source():
    sys.stderr.write("Missing engine source files! To generate the engine solution make sure to get the source from github.com/CRYTEK/CRYENGINE/.\n")
    sys.exit(602)

def error_winreg_not_available():
    sys.stderr.write("Unable to load module winreg! This module is required to automatically generate C++ solutions.")
    sys.exit(603)

def error_unable_to_replace_file(path):
    sys.stderr.write("Unable to replace file '%s'. Please remove the file manually.\n" % path)
    sys.exit(610)

def error_engine_tool_not_found(path):
    sys.stderr.write("'%s' not found.\n" % path)
    sys.exit(620)

def error_cmake_not_found():
    sys.stderr.write("Unable to locate CMake.\nPlease download and install CMake from https://cmake.org/download/ and make sure it is available through the PATH environment variable.\n")
    sys.exit(621)

def error_code_folder_not_specified():
    sys.stderr.write("The code folder has not been specified in the project file!\n")
    sys.exit(642)

def print_subprocess(cmd):
    print(' '.join(map(lambda a: '"%s"' % a, cmd)))

#---

def get_cmake_exe_path():
    return os.path.join(get_cmake_dir(), 'Win32/bin/cmake.exe')

def get_cmake_dir():
    return os.path.join(get_engine_path(), 'Tools', 'CMake')

def get_tools_path():
    if getattr(sys, 'frozen', False):
        script_path = sys.executable
    else:
        script_path = __file__

    return os.path.abspath(os.path.dirname(script_path))

def get_engine_path():
    return os.path.abspath(os.path.join(get_tools_path(), '..', '..'))

def get_project_solution_dir(project_path):
    x64 = os.path.join("Solutions", "win64")
    x86 = os.path.join("Solutions", "win32")

    if os.path.isdir(os.path.join(project_path, x64)):
        return x64

    if os.path.isdir(os.path.join(project_path, x86)):
        return x86

    return None

#-- BUILD ---

def cmd_build(args):
    if not os.path.isfile(args.project_file):
        error_project_not_found(args.project_file)

    project = cryproject.load(args.project_file)
    if project is None:
        error_project_json_decode(args.project_file)

    cmake_path = get_cmake_exe_path()
    if cmake_path is None:
        error_cmake_not_found()

    #--- cmake
    if cryproject.cmakelists_dir(project) is not None:
        project_path = os.path.dirname(os.path.abspath(args.project_file))
        solution_dir = get_project_solution_dir(project_path)

        subcmd = (
            cmake_path,
            '--build', solution_dir,
            '--config', args.config
        )

        print_subprocess(subcmd)
        errcode = subprocess.call(subcmd, cwd=project_path)
        if errcode != 0:
            sys.exit(errcode)

#--- PROJGEN ---

def cmd_cmake_gui(args):
    if not os.path.isfile(args.project_file):
        error_project_not_found(args.project_file)

    project = cryproject.load(args.project_file)
    if project is None:
        error_project_json_decode(args.project_file)
    dirname = os.path.dirname(os.path.abspath(args.project_file))

    #--- cpp
    cmakelists_dir = cryproject.cmakelists_dir(project)
    if cmakelists_dir is not None:
        project_path = os.path.abspath(os.path.dirname(args.project_file))
        source_path = os.path.join(project_path, cmakelists_dir)
        solution_dir = get_project_solution_dir(project_path)
        if solution_dir is None:
            args.buildmachine = False
            cmd_projgen(args, True)
        else:
            solution_path = os.path.join(project_path, solution_dir)
            open_cmake_gui(source_path, solution_path)

def open_cmake_gui(source_dir, build_dir):
    cmake_path = get_cmake_exe_path()
    if cmake_path is None:
        error_cmake_not_found()

    cmake_gui_path = cmake_path.replace('cmake.exe', 'cmake-gui.exe')
    if not os.path.isfile(cmake_gui_path):
        error_cmake_not_found()

    command_str = '"{}" -H"{}" -B"{}"'.format(cmake_gui_path, source_dir, build_dir)
    subprocess.Popen(command_str, cwd=source_dir)

def cmd_projgen(args, open_cmake=False):
    if not os.path.isfile(args.project_file):
        error_project_not_found(args.project_file)

    project = cryproject.load(args.project_file)
    if project is None:
        error_project_json_decode(args.project_file)

    project_path = os.path.abspath(os.path.dirname(args.project_file))
    engine_path = get_engine_path()

    cmakelists_dir = cryproject.cmakelists_dir(project)

    if not cmakelists_dir:
        error_code_folder_not_specified()

    code_directory = os.path.join(project_path, cmakelists_dir)

    # Generate solutions
    crysolutiongenerator.generate_solution(args.project_file, code_directory, engine_path)

    # Skip on Crytek build agents
    if args.buildmachine:
        return

    cmakelists_path = os.path.join(os.path.join(project_path, cmakelists_dir), 'CMakeLists.txt')

    # Generate the Solution
    if code_directory is not None and os.path.isfile(cmakelists_path):
        generate_project_solution(project_path, code_directory, open_cmake)

def cmd_engine_gen(args):
    if not os.path.isfile(args.engine_file):
        error_engine_not_found(args.engine_file)

    engine_path = get_engine_path()

    # Check if the CrySystem folder is available, which indicates the source code is available
    source_dir = os.path.join(engine_path, "Code", "CryEngine", "CrySystem")
    if not os.path.isdir(source_dir):
        error_missing_engine_source()

    # Generate the Solution, skip on Crytek build agents
    if not args.buildmachine:
        generate_engine_solution(engine_path)

def generate_project_solution(project_path, cmakelists_dir, open_cmake=False):
    """
    Opens the configurations selection UI, and generates the project solution with the selected configuration.
    """
    if not HAS_WIN_MODULES:
        error_winreg_not_available()

    configs = [
        #Visual Studio 14 2015 Express
        {
            'title':'Visual Studio 2015 Express Win64',
            'cmake_toolchain': 'toolchain/windows/WindowsPC-MSVC.cmake',
            'cmake_generator': 'Visual Studio 14 2015 Win64',
            'cmake_builddir': 'solutions/win64',
            'compiler':{'reg_key': winreg.HKEY_CLASSES_ROOT, 'key_path': r'\WDExpress.DTE.14.0'}
        },
        {
            'title':'Visual Studio 2015 Express Win32',
            'cmake_toolchain': 'toolchain/windows/WindowsPC-MSVC.cmake',
            'cmake_generator': 'Visual Studio 14 2015',
            'cmake_builddir': 'solutions/win32',
            'compiler':{'reg_key': winreg.HKEY_CLASSES_ROOT, 'key_path': r'\WDExpress.DTE.14.0'}
        },

        #Visual Studio 14 2015
        {
            'title':'Visual Studio 2015 Win64',
            'cmake_toolchain': 'toolchain/windows/WindowsPC-MSVC.cmake',
            'cmake_generator': 'Visual Studio 14 2015 Win64',
            'cmake_builddir': 'solutions/win64',
            'compiler':{'reg_key': winreg.HKEY_CLASSES_ROOT, 'key_path': r'\VisualStudio.DTE.14.0'}
        },
        {
            'title':'Visual Studio 2015 Win32',
            'cmake_toolchain': 'toolchain/windows/WindowsPC-MSVC.cmake',
            'cmake_generator': 'Visual Studio 14 2015',
            'cmake_builddir': 'solutions/win32',
            'compiler':{'reg_key': winreg.HKEY_CLASSES_ROOT, 'key_path': r'\VisualStudio.DTE.14.0'}
        },

        #Visual Studio 15 2017
        {
            'title':'Visual Studio 2017 Win64',
            'cmake_toolchain': 'toolchain/windows/WindowsPC-MSVC.cmake',
            'cmake_generator': 'Visual Studio 15 2017 Win64',
            'cmake_builddir': 'solutions/win64',
            'compiler':{'reg_key': winreg.HKEY_CLASSES_ROOT, 'key_path': r'\VisualStudio.DTE.15.0'}
        },
        {
            'title':'Visual Studio 2017 Win32',
            'cmake_toolchain': 'toolchain/windows/WindowsPC-MSVC.cmake',
            'cmake_generator': 'Visual Studio 15 2017',
            'cmake_builddir': 'solutions/win32',
            'compiler':{'reg_key': winreg.HKEY_CLASSES_ROOT, 'key_path': r'\VisualStudio.DTE.15.0'}
        }
    ]

    # Run the GUI to select a config for CMake.
    config = cryrun_gui.select_config(configs)

    #No config means the user canceled while selecting the config, so we can safely exit.
    if not config:
        sys.exit(0)

    generate_solution(project_path, cmakelists_dir, config, open_cmake)

def generate_engine_solution(engine_path):
    """
    Opens the configurations selection UI, and generates the engine solution with the selected configuration.
    """
    if not HAS_WIN_MODULES:
        error_winreg_not_available()

    configs = [
        #Visual Studio 14 2015 Express
        {
            'title':'Visual Studio 2015 Express Win64',
            'cmake_toolchain': 'toolchain/windows/WindowsPC-MSVC.cmake',
            'cmake_generator': 'Visual Studio 14 2015 Win64',
            'cmake_builddir': 'solutions/win64',
            'compiler':{'reg_key': winreg.HKEY_CLASSES_ROOT, 'key_path': r'\WDExpress.DTE.14.0'}
        },
        {
            'title':'Visual Studio 2015 Express Win32',
            'cmake_toolchain': 'toolchain/windows/WindowsPC-MSVC.cmake',
            'cmake_generator': 'Visual Studio 14 2015',
            'cmake_builddir': 'solutions/win32',
            'compiler':{'reg_key': winreg.HKEY_CLASSES_ROOT, 'key_path': r'\WDExpress.DTE.14.0'}
        },

        #Visual Studio 14 2015
        {
            'title':'Visual Studio 2015 Win64',
            'cmake_toolchain': 'toolchain/windows/WindowsPC-MSVC.cmake',
            'cmake_generator': 'Visual Studio 14 2015 Win64',
            'cmake_builddir': 'solutions_cmake/win64',
            'compiler':{'reg_key': winreg.HKEY_CLASSES_ROOT, 'key_path': r'\VisualStudio.DTE.14.0'}
        },
        {
            'title':'Visual Studio 2015 Win32',
            'cmake_toolchain': 'toolchain/windows/WindowsPC-MSVC.cmake',
            'cmake_generator': 'Visual Studio 14 2015',
            'cmake_builddir': 'solutions_cmake/win32',
            'compiler':{'reg_key': winreg.HKEY_CLASSES_ROOT, 'key_path': r'\VisualStudio.DTE.14.0'}
        },

        #Visual Studio 15 2017
        {
            'title':'Visual Studio 2017 Win64',
            'cmake_toolchain': 'toolchain/windows/WindowsPC-MSVC.cmake',
            'cmake_generator': 'Visual Studio 15 2017 Win64',
            'cmake_builddir': 'solutions_cmake/win64',
            'compiler':{'reg_key': winreg.HKEY_CLASSES_ROOT, 'key_path': r'\VisualStudio.DTE.15.0'}
        },
        {
            'title':'Visual Studio 2017 Win32',
            'cmake_toolchain': 'toolchain/windows/WindowsPC-MSVC.cmake',
            'cmake_generator': 'Visual Studio 15 2017',
            'cmake_builddir': 'solutions_cmake/win32',
            'compiler':{'reg_key': winreg.HKEY_CLASSES_ROOT, 'key_path': r'\VisualStudio.DTE.15.0'}
        }
    ]

    # Run the GUI to select a config for CMake.
    config = cryrun_gui.select_config(configs)

    #No config means the user canceled while selecting the config, so we can safely exit.
    if not config:
        sys.exit(0)

    generate_solution(engine_path, engine_path, config, True)

def generate_solution(working_directory, cmakelists_dir, config, open_gui):
    cmake_dir = get_cmake_dir()
    cmake_path = get_cmake_exe_path()

    if cmake_path is None:
        error_cmake_not_found()

    # By default the CMake output is hidden. This is printed to make sure the user knows it's not stuck.
    print("Generating solution...")

    toolchain = config['cmake_toolchain']
    solution_path = os.path.join(working_directory, config['cmake_builddir'])
    print("Solution path: {}".format(solution_path))
    generator = config['cmake_generator']

    check_cmake_cache(solution_path, generator)
    if not os.path.isdir(solution_path):
        os.makedirs(solution_path)

    if toolchain:
        toolchain = toolchain.replace('\\', '/')
        toolchain = os.path.join(cmake_dir, toolchain)

    cmake_command = ['"{}"'.format(cmake_path)]
    cmake_command.append('-Wno-dev')
    cmake_command.append('-G"{}"'.format(generator))
    if toolchain:
        cmake_command.append('-DCMAKE_TOOLCHAIN_FILE="{}"'.format(toolchain))
    cmake_command.append('"{}"'.format(cmakelists_dir))

    # Filter empty commands, and convert the list to a string.
    cmake_command = list(filter(bool, cmake_command))
    command_str = ("".join("{} ".format(e) for e in cmake_command)).strip()

    try:
        subprocess.run(command_str, cwd=solution_path, stdout=None, stderr=subprocess.PIPE, check=True, universal_newlines=True)
    except subprocess.CalledProcessError as e:
        if not e.returncode == 0:
            print("Encountered and error while running command '{}'!".format(e.cmd))
            print("Error: {}".format(e.stderr))
            print("Generating solution has failed!")
            print("Press Enter to exit")
            input()
            sys.exit(0)

    if open_gui:
        open_cmake_gui(cmakelists_dir, solution_path)

def check_cmake_cache(solution_path, generator):
    """
    Checks the line CMAKE_GENERATOR:INTERNAL in the cmake_cache.
    If this line is different than the selected generator, the cache will be deleted.
    """
    cache_file = os.path.join(solution_path, "CMakeCache.txt")
    if not os.path.isfile(cache_file):
        return

    pattern = "CMAKE_GENERATOR:INTERNAL="
    found = False

    with open(cache_file) as file:
        for line in file:
            if line.startswith(pattern):
                current_generator = line.rpartition("=")[2].rstrip()
                if current_generator == generator:
                    return
                else:
                    found = True
                    break

    if found:
        print("Generator has changed. Deleting {}".format(solution_path))
        remove_directory(solution_path)

def remove_directory(directory):
    if os.path.exists(directory):
        shutil.rmtree(directory, onerror=on_rm_error)

def on_rm_error(func, path, exc_info):
    """
    Clears the read-only flag of the file at path, and unlinks it.
    :param func: The function that raised the exception.
    :param path: The path of the file that couldn't be removed.
    :param exc_info: The exception information returned by sys.exc_info().
    """
    os.chmod(path, stat.S_IWRITE)
    os.unlink(path)

#--- OPEN ---

def cmd_open(args):
    if not os.path.isfile(args.project_file):
        error_project_not_found(args.project_file)

    project = cryproject.load(args.project_file)
    if project is None:
        error_project_json_decode(args.project_file)

    tool_path = os.path.join(get_engine_path(), 'bin', args.platform, 'GameLauncher.exe')
    if not os.path.isfile(tool_path):
        error_engine_tool_not_found(tool_path)

    #---

    subcmd = (
        tool_path,
        '-project',
        os.path.abspath(args.project_file)
    )

    print_subprocess(subcmd)
    subprocess.Popen(subcmd)

#--- DEDICATED SERVER ---

def cmd_launch_dedicated_server(args):
    if not os.path.isfile(args.project_file):
        error_project_not_found(args.project_file)

    project = cryproject.load(args.project_file)
    if project is None:
        error_project_json_decode(args.project_file)

    tool_path = os.path.join(get_engine_path(), 'bin', args.platform, 'Game_Server.exe')
    if not os.path.isfile(tool_path):
        error_engine_tool_not_found(tool_path)

    #---

    subcmd = (
        tool_path,
        '-project',
        os.path.abspath(args.project_file)
    )

    print_subprocess(subcmd)
    subprocess.Popen(subcmd)

#--- PACKAGE ---

def cmd_package(args):
    if not os.path.isfile(args.project_file):
        error_project_not_found(args.project_file)

    release_project.run(args.project_file)


#--- EDIT ---
def cmd_edit(args):
    if not os.path.isfile(args.project_file):
        error_project_not_found(args.project_file)

    project = cryproject.load(args.project_file)
    if project is None:
        error_project_json_decode(args.project_file)

    tool_path = os.path.join(get_engine_path(), 'bin', args.platform, 'Sandbox.exe')
    if not os.path.isfile(tool_path):
        error_engine_tool_not_found(tool_path)

    #---

    subcmd = (
        tool_path,
        '-project',
        os.path.abspath(args.project_file)
    )

    print_subprocess(subcmd)
    subprocess.Popen(subcmd)

#--- UPGRADE ---

def upgrade_identify50(project_file):
    dirname = os.path.dirname(project_file)

    listdir = os.listdir(os.path.join(dirname, 'Code'))
    if all((filename in listdir) for filename in ('CESharp', 'EditorApp', 'Game', 'SandboxInteraction', 'SandboxInteraction.sln')):
        return ('cs', 'SandboxInteraction')

    if all((filename in listdir) for filename in ('CESharp', 'Game', 'Sydewinder', 'Sydewinder.sln')):
        return ('cs', 'Sydewinder')

    if all((filename in listdir) for filename in ('CESharp', 'Game')):
        if os.path.isdir(os.path.join(dirname, 'Code', 'CESharp', 'SampleApp')):
            return ('cs', 'Blank')

    if all((filename in listdir) for filename in ('Game', )):
        if os.path.isdir(os.path.join(dirname, 'Assets', 'levels', 'example')):
            return ('cpp', 'Blank')

    return None

def upgrade_identify51(project_file):
    dirname = os.path.dirname(project_file)

    listdir = os.listdir(os.path.join(dirname, 'Code'))
    if all((filename in listdir) for filename in ('Game', 'SampleApp', 'SampleApp.sln')):
        return ('cs', 'Blank')

    if all((filename in listdir) for filename in ('Game', 'EditorApp', 'SandboxInteraction', 'SandboxInteraction.sln')):
        return ('cs', 'SandboxInteraction')

    if all((filename in listdir) for filename in ('Game', 'Sydewinder', 'Sydewinder.sln')):
        return ('cs', 'Sydewinder')

    if all((filename in listdir) for filename in ('Game', )):
        if os.path.isdir(os.path.join(dirname, 'Assets', 'levels', 'example')):
            return ('cpp', 'Blank')

    return None

def cmd_upgrade(args):
    if not os.path.isfile(args.project_file):
        error_project_not_found(args.project_file)

    try:
        file = open(args.project_file, 'r')
        project = configparser.ConfigParser()
        project.read_string('[project]\n' + file.read())
        file.close()
    except ValueError:
        error_project_json_decode(args.project_file)

    engine_version = project['project'].get('engine_version')
    restore_version = {
        '5.0.0': '5.0.0',
        '5.1.0': '5.1.0',
        '5.1.1': '5.1.0'
    }.get(engine_version)
    if restore_version is None:
        error_upgrade_engine_version(engine_version)

    template_name = None
    if restore_version == '5.0.0':
        template_name = upgrade_identify50(args.project_file)
    elif restore_version == '5.1.0':
        template_name = upgrade_identify51(args.project_file)

    if template_name is None:
        error_upgrade_template_unknown(args.project_file)

    restore_path = os.path.abspath(os.path.join(get_tools_path(), 'upgrade', restore_version, *template_name) + '.zip')
    if not os.path.isfile(restore_path):
        error_upgrade_template_missing(restore_path)

    #---

    (dirname, basename) = os.path.split(os.path.abspath(args.project_file))
    prevcwd = os.getcwd()
    os.chdir(dirname)

    if not os.path.isdir('Backup'):
        os.mkdir('Backup')
    (fd, zfilename) = tempfile.mkstemp('.zip', datetime.date.today().strftime('upgrade_%y%m%d_'), os.path.join('Backup', dirname))
    file = os.fdopen(fd, 'wb')
    backup = zipfile.ZipFile(file, 'w', zipfile.ZIP_DEFLATED)
    restore = zipfile.ZipFile(restore_path, 'r')

    #.bat
    for filename in (basename, 'Code_CPP.bat', 'Code_CS.bat', 'Editor.bat', 'Game.bat', 'CMakeLists.txt'):
        if os.path.isfile(filename):
            backup.write(filename)

    #bin
    for (dirpath, dirnames, filenames) in os.walk('bin'):
        for filename in filter(lambda a: os.path.splitext(a)[1] in ('.exe', '.dll'), filenames):
            backup.write(os.path.join(dirpath, filename))

    #Solutions
    for (dirpath, dirnames, filenames) in os.walk('Code'):
        for filename in filter(lambda a: os.path.splitext(a)[1] in ('.sln', '.vcxproj', '.filters', '.user', '.csproj'), filenames):
            path = os.path.join(dirpath, filename)
            backup.write(path)

    backup_list = backup.namelist()

    #Files to be restored
    for filename in restore.namelist():
        if os.path.isfile(filename) and filename not in backup_list:
            backup.write(filename)

    #---

    backup.close()
    file.close()

    #Delete files backed up

    z = zipfile.ZipFile(zfilename, 'r')
    for filename in z.namelist():
        os.chmod(filename, stat.S_IWRITE)
        os.remove(filename)
    z.close()

    restore.extractall()
    restore.close()
    os.chdir(prevcwd)

#--- REQUIRE ---

def require_getall(registry, require_list, result):

    for k in require_list:
        if k in result:
            continue

        project_file = cryregistry.project_file(registry, k)
        project = cryproject.load(project_file)
        result[k] = cryproject.require_list(project)

def require_sortedlist(d):
    d = dict(d)

    result = []
    while d:
        empty = [k for(k, v) in d.items() if len(v) == 0]
        for k in empty:
            del d[k]

        for key in d.keys():
            d[key] = list(filter(lambda k: k not in empty, d[key]))

        empty.sort()
        result.extend(empty)

    return result

def cmd_require(args):
    registry = cryregistry.load()
    project = cryproject.load(args.project_file)

    plugin_dependencies = {}
    require_getall(registry, cryproject.require_list(project), plugin_dependencies)
    plugin_list = require_sortedlist(plugin_dependencies)
    plugin_list = cryregistry.filter_plugin(registry, plugin_list)

    project_path = os.path.dirname(args.project_file)
    plugin_path = os.path.join(project_path, 'cryext.txt')
    if os.path.isfile(plugin_path):
        os.remove(plugin_path)

    plugin_file = open(plugin_path, 'w')
    for k in plugin_list:
        project_file = cryregistry.project_file(registry, k)
        project_path = os.path.dirname(project_file)
        project = cryproject.load(project_file)

        (m_extensionName, shared_path) = cryproject.shared_tuple(project, args.platform, args.config)
        asset_dir = cryproject.asset_dir(project)

        m_extensionBinaryPath = os.path.normpath(os.path.join(project_path, shared_path))
        m_extensionAssetDirectory = asset_dir and os.path.normpath(os.path.join(project_path, asset_dir)) or ''
        m_extensionClassName = 'EngineExtension_%s' % os.path.splitext(os.path.basename(m_extensionBinaryPath))[0]

        line = ';'.join((m_extensionName, m_extensionClassName, m_extensionBinaryPath, m_extensionAssetDirectory))
        plugin_file.write(line + os.linesep)

    plugin_file.close()

#--- METAGEN ---

def cmd_metagen(args):
    if not os.path.isfile(args.project_file):
        error_project_not_found(args.project_file)

    project = cryproject.load(args.project_file)
    if project is None:
        error_project_json_decode(args.project_file)

    tool_path = os.path.join(get_engine_path(), 'Tools/rc/rc.exe')
    if not os.path.isfile(tool_path):
        error_engine_tool_not_found(tool_path)

    job_path = os.path.join(get_engine_path(), 'Tools/cryassets/rcjob_cryassets.xml')
    if not os.path.isfile(job_path):
        error_engine_tool_not_found(job_path)

    project_path = os.path.dirname(os.path.abspath(args.project_file))
    asset_dir = cryproject.asset_dir(project)
    asset_path = os.path.normpath(os.path.join(project_path, asset_dir))

    subcmd = (
        tool_path,
        ('/job=' + job_path),
        ('/src=' + asset_path)
    )

    print_subprocess(subcmd)
    subprocess.Popen(subcmd)

#--- MAIN ---
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--platform', default='win_x64', choices=('win_x86', 'win_x64'))
    parser.add_argument('--config', default='RelWithDebInfo', choices=('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel'))

    subparsers = parser.add_subparsers(dest='command')
    subparsers.required = True

    parser_upgrade = subparsers.add_parser('upgrade')
    parser_upgrade.add_argument('project_file')
    parser_upgrade.add_argument('--engine_version')
    parser_upgrade.set_defaults(func=cmd_upgrade)

    parser_require = subparsers.add_parser('require')
    parser_require.add_argument('project_file')
    parser_require.set_defaults(func=cmd_require)

    parser_engine_gen = subparsers.add_parser('engine_gen')
    parser_engine_gen.add_argument('engine_file')
    parser_engine_gen.add_argument('--buildmachine', action='store_true', default=False)
    parser_engine_gen.set_defaults(func=cmd_engine_gen)

    parser_projgen = subparsers.add_parser('projgen')
    parser_projgen.add_argument('project_file')
    parser_projgen.add_argument('--buildmachine', action='store_true', default=False)
    parser_projgen.set_defaults(func=cmd_projgen)

    parser_projgen = subparsers.add_parser('cmake-gui')
    parser_projgen.add_argument('project_file')
    parser_projgen.set_defaults(func=cmd_cmake_gui)

    parser_build = subparsers.add_parser('build')
    parser_build.add_argument('project_file')
    parser_build.set_defaults(func=cmd_build)

    parser_open = subparsers.add_parser('open')
    parser_open.add_argument('project_file')
    parser_open.set_defaults(func=cmd_open)

    parser_server = subparsers.add_parser('server')
    parser_server.add_argument('project_file')
    parser_server.set_defaults(func=cmd_launch_dedicated_server)

    parser_edit = subparsers.add_parser('edit')
    parser_edit.add_argument('project_file')
    parser_edit.set_defaults(func=cmd_edit)

    parser_package = subparsers.add_parser('package')
    parser_package.add_argument('project_file')
    parser_package.set_defaults(func=cmd_package)


    parser_edit = subparsers.add_parser('metagen')
    parser_edit.add_argument('project_file')
    parser_edit.set_defaults(func=cmd_metagen)

    args = parser.parse_args()
    args.func(args)

if __name__ == '__main__':
    main()
