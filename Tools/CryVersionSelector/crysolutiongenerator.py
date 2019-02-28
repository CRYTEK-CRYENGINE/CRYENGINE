#!/usr/bin/env python3

import os
import os.path
import uuid
import glob

from string import Template

has_win_modules = True
try:
    from win32com.client import Dispatch
except ImportError:
    has_win_modules = False

def generate_solution (project_file, code_directory, engine_root_directory):
    project_name = os.path.splitext(os.path.basename(project_file))[0]

    generate_csharp_solution(project_name, project_file, code_directory, engine_root_directory)

    # Now generate the CMakeLists.txt for C++ development
    generate_cpp_cmakelists(project_name, project_file, code_directory, engine_root_directory)

def generate_csharp_solution (project_name, project_file, code_directory, engine_root_directory):
    solution_file_template = Template("""Microsoft Visual Studio Solution File, Format Version 12.00
# Visual Studio 15
VisualStudioVersion = 15.0.27130.2003
MinimumVisualStudioVersion = 10.0.40219.1
$projects
Global
    GlobalSection(SolutionConfigurationPlatforms) = preSolution
        Debug|Any CPU = Debug|Any CPU
        Release|Any CPU = Release|Any CPU
    EndGlobalSection
    GlobalSection(ProjectConfigurationPlatforms) = postSolution
        $project_configurations
    EndGlobalSection
    GlobalSection(SolutionProperties) = preSolution
        HideSolutionNode = FALSE
    EndGlobalSection
EndGlobal""")

    projects = ""
    project_configurations = ""

    # Start by generation the Solution file for C# source directories
    for entry in os.scandir(code_directory):
        if os.path.isdir(entry.path) and not os.path.basename(entry.path).startswith("."):
            csproject_guid = uuid.uuid4()

            if generate_csharp_project(project_file, code_directory, os.path.basename(entry.path), engine_root_directory, csproject_guid):
                project_template = Template('Project("{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}") = "$project_name", "$project_name\$project_name.csproj", "{$csproject_guid}"')

                projects += project_template.substitute({'project_name': os.path.basename(entry.path), 'csproject_guid': csproject_guid})
                projects += "\nEndProject\n"

                configuration_template = Template("""{$csproject_guid}.Debug|Any CPU.ActiveCfg = Debug|Any CPU
        {$csproject_guid}.Debug|Any CPU.Build.0 = Debug|Any CPU
        {$csproject_guid}.Release|Any CPU.ActiveCfg = Release|Any CPU
        {$csproject_guid}.Release|Any CPU.Build.0 = Release|Any CPU""")

                project_configurations += configuration_template.substitute({'csproject_guid': csproject_guid})

    if len(projects) > 0:
        solution_file_contents = solution_file_template.substitute({ 'projects': projects, 'project_configurations': project_configurations})
        solution_file_path = os.path.join(code_directory, project_name + ".sln")
        solution_file = open(solution_file_path, 'w')
        solution_file.write(solution_file_contents)
        create_shortcut(solution_file_path, project_file)

def generate_csharp_project (project_file, code_directory, cs_source_directory, engine_root_directory, csproject_guid):
    cs_project_file_template = Template("""<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="14.0" DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <Import Project="$$(MSBuildExtensionsPath)\$$(MSBuildToolsVersion)\Microsoft.Common.props" Condition="Exists('$$(MSBuildExtensionsPath)\$$(MSBuildToolsVersion)\Microsoft.Common.props')" />
  <PropertyGroup>
    <Configuration Condition=" '$$(Configuration)' == '' ">Debug</Configuration>
    <Platform Condition=" '$$(Platform)' == '' ">AnyCPU</Platform>
    <ProjectTypeGuids>{E7562513-36BA-4D11-A927-975E5375ED10};{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}</ProjectTypeGuids>
    <ProjectGuid>$csproject_guid</ProjectGuid>
    <OutputType>Library</OutputType>
    <AppDesignerFolder>Properties</AppDesignerFolder>
    <RootNamespace>CryEngine.Projects.$project_name</RootNamespace>
    <AssemblyName>$project_name</AssemblyName>
    <TargetFrameworkVersion>v4.5.2</TargetFrameworkVersion>
    <FileAlignment>512</FileAlignment>
    <UseMSBuildEngine>false</UseMSBuildEngine>
    <CryEngineLauncherPath>$engine_bin_directory\GameLauncher.exe</CryEngineLauncherPath>
    <CryEngineProjectPath>$project_file</CryEngineProjectPath>
    <CryEngineCommandArguments></CryEngineCommandArguments>
  </PropertyGroup>
  <PropertyGroup Condition=" '$$(Configuration)|$$(Platform)' == 'Debug|AnyCPU' ">
    <DebugSymbols>true</DebugSymbols>
    <DebugType>portable</DebugType>
    <Optimize>false</Optimize>
    <OutputPath>$output_path</OutputPath>
    <DefineConstants>DEBUG;TRACE</DefineConstants>
    <ErrorReport>prompt</ErrorReport>
    <WarningLevel>4</WarningLevel>
  </PropertyGroup>
  <PropertyGroup Condition=" '$$(Configuration)|$$(Platform)' == 'Release|AnyCPU' ">
    <DebugType>none</DebugType>
    <Optimize>true</Optimize>
    <OutputPath>$output_path</OutputPath>
    <DefineConstants>TRACE</DefineConstants>
    <ErrorReport>prompt</ErrorReport>
    <WarningLevel>4</WarningLevel>
  </PropertyGroup>
  <ItemGroup>
    $references
  </ItemGroup>
  <ItemGroup>
    $includes
  </ItemGroup>
  <Import Project="$$(MSBuildToolsPath)\Microsoft.CSharp.targets" />
  <!-- To modify your build process, add your task inside one of the targets below and uncomment it.
       Other similar extension points exist, see Microsoft.Common.targets.
  <Target Name="BeforeBuild">
  </Target>
  <Target Name="AfterBuild">
  </Target>
  -->

</Project>
""")

    includes = ""

    csproj_directory = os.path.join(code_directory, cs_source_directory)

    for filename in glob.iglob(csproj_directory + "/**/*.cs", recursive=True):
        if not os.path.relpath(os.path.abspath(os.path.join(os.path.dirname(filename), os.pardir)), csproj_directory) == 'obj': # Ignore the obj directory generated by building
            includes += '<Compile Include="' + os.path.relpath(filename, csproj_directory) + '" />\n    '

    if len(includes) == 0:
        return False

    engine_bin_directory = os.path.join(engine_root_directory, 'bin', 'win_x64')

    references = Template('''<Reference Include="System" />
    <Reference Include="System.XML" />
    <Reference Include="System.Core" />
    <Reference Include="System.Runtime.Serialization" />
    <Reference Include="System.Xml.Linq" />
    <Reference Include="CryEngine.Common">
      <HintPath>$engine_bin_directory\CryEngine.Common.dll</HintPath>
      <Private>False</Private>
    </Reference>
    <Reference Include="CryEngine.Core">
      <HintPath>$engine_bin_directory\CryEngine.Core.dll</HintPath>
      <Private>False</Private>
    </Reference>
    <Reference Include="CryEngine.Core.UI">
      <HintPath>$engine_bin_directory\CryEngine.Core.UI.dll</HintPath>
      <Private>False</Private>
    </Reference>
''').substitute({'engine_bin_directory': engine_bin_directory})

    output_path = os.path.abspath(os.path.join(code_directory, os.pardir, "bin"))

    cs_project_file_contents = cs_project_file_template.substitute({ 'csproject_guid':  csproject_guid, 'project_name': cs_source_directory, 'includes': includes, 'references': references, 'output_path': output_path, 'engine_bin_directory': engine_bin_directory, 'project_file': project_file})

    cs_project_file = open(os.path.join(csproj_directory, cs_source_directory) + ".csproj", 'w')
    cs_project_file.write(cs_project_file_contents)

    return True

def generate_cpp_cmakelists (project_name, project_file, code_directory, engine_root_directory, is_default_project=True):
    cmakelists_template = Template("cmake_minimum_required (VERSION 3.6.2)\n")
    project_file_name = os.path.basename(project_file)

    if is_default_project:
         cmakelists_template.template += """set(CRYENGINE_DIR "$engine_root_directory")
set(TOOLS_CMAKE_DIR "$${CRYENGINE_DIR}/Tools/CMake")

set(PROJECT_BUILD 1)
set(PROJECT_DIR "$${CMAKE_SOURCE_DIR}/../")

include("$${TOOLS_CMAKE_DIR}/CommonOptions.cmake")

add_subdirectory("$${CRYENGINE_DIR}" "$${CMAKE_CURRENT_BINARY_DIR}/CRYENGINE")

include("$${TOOLS_CMAKE_DIR}/Configure.cmake")"""

    cmakelists_template.template += """\nstart_sources()

sources_platform(ALL)
$sources
end_sources()

CryEngineModule($project_name FORCE_SHARED PCH "StdAfx.cpp" SOLUTION_FOLDER "Project")

target_include_directories($${THIS_PROJECT}
PRIVATE
    "$${CRYENGINE_DIR}/Code/CryEngine/CryCommon"
    "$${CRYENGINE_DIR}/Code/CryEngine/CryAction"
    "$${CRYENGINE_DIR}/Code/CryEngine/CrySchematyc/Core/Interface"
    "$${CRYENGINE_DIR}/Code/CryPlugins/CryDefaultEntities/Module"
)
"""

    if is_default_project:
        cmakelists_template.template += '''\n$launcher_projects
# Set StartUp project in Visual Studio
set_solution_startup_target($startup_project)

if (WIN32)
    set_visual_studio_debugger_command( $${THIS_PROJECT} "$${CRYENGINE_DIR}/bin/win_x64/GameLauncher.exe" "-project \\"$${PROJECT_DIR}$projectfile\\"" )
endif()\n'''

    cmakelists_path = os.path.join(code_directory, 'CMakeLists.txt')

    custom_block_prefix = '#BEGIN-CUSTOM'
    custom_block_postfix = '#END-CUSTOM'

    custom_contents = ""

    standalone_directories = []

    # Try to copy custom data
    if os.path.exists(cmakelists_path):
        cmakelists_file = open(cmakelists_path, 'r')

        existing_cmakelists_contents = cmakelists_file.read()

        current_index = 0
        while True:
            current_index = existing_cmakelists_contents.find(custom_block_prefix, current_index)
            if current_index == -1:
                break

            end_index = existing_cmakelists_contents.find(custom_block_postfix, current_index + len(custom_block_prefix))
            if end_index == -1:
                break

            end_index += len(custom_block_postfix)

            custom_block_contents = existing_cmakelists_contents[current_index:end_index]

            # Check for use of add_subdirectory
            add_subdirectory_index = 0
            while True:
                add_subdirectory_index = custom_block_contents.find("add_subdirectory", add_subdirectory_index)
                if add_subdirectory_index == -1:
                    break

                add_subdirectory_index += len("add_subdirectory")

                add_subdirectory_index = custom_block_contents.find('(', add_subdirectory_index) + 1
                close_scope_index = custom_block_contents.find(')', add_subdirectory_index)

                standalone_directory = custom_block_contents[add_subdirectory_index : close_scope_index]

                standalone_directory = standalone_directory.replace('"', '')

                standalone_project_name = standalone_directory

                # Only ignore the first directory
                first_delimiter_index = standalone_project_name.find('/')
                if first_delimiter_index != -1:
                    standalone_project_name = standalone_project_name[:first_delimiter_index]

                generate_cpp_cmakelists(standalone_project_name, project_file_name, os.path.join(code_directory, standalone_directory), engine_root_directory, False)

                standalone_directories.append(standalone_project_name)

            custom_contents += '\n' + custom_block_contents
            current_index = end_index
    else:
        custom_contents += '\n' + custom_block_prefix + '\n# Make any custom changes here, modifications outside of the block will be discarded on regeneration.\n'+ custom_block_postfix

    cmakelists_sources = ""

    source_count = 0

    directory_sources = add_cpp_sources(code_directory, project_name, code_directory, standalone_directories)
    if directory_sources != "":
        source_count += 1
        cmakelists_sources += directory_sources

    if source_count == 0:
        return

    startup_project, launcher_projects = get_startup_and_launcher_projects(project_name, project_file_name, engine_root_directory)

    output_path = os.path.abspath(os.path.join(code_directory, os.pardir, "bin", "win_x64"))

    cmakelists_contents = cmakelists_template.substitute({
        'sources' : cmakelists_sources,
        'engine_root_directory': engine_root_directory.replace('\\', '/'),
        'project_name': project_name,
        'projectfile': project_file_name,
        'project_path': os.path.abspath(os.path.dirname(project_file)).replace('\\', '/'),
        'startup_project': startup_project,
        'launcher_projects': launcher_projects,
        'output_path': output_path.replace('\\', '/') })
    cmakelists_contents += custom_contents

    cmakelists_file = open(cmakelists_path, 'w')

    cmakelists_file.write(cmakelists_contents)

def get_startup_and_launcher_projects(project_name, project_file_name, engine_root_directory):
    platform_file_path = os.path.join(engine_root_directory, "Code", "CryEngine", "CryCommon", "CryCore", "Platform", "platform.h")
    if not os.path.isfile(platform_file_path):
        print("File not found: {}".format(platform_file_path))
        return (project_name, "")

    launcher_projects = Template('''
if(OPTION_ENGINE)
    if(NOT EXISTS "$${CRYENGINE_DIR}/Code/Sandbox/EditorQt")
		add_library(Editor STATIC "$${CRYENGINE_DIR}/Code/CryEngine/CryCommon/CryCore/Platform/platform.h")
		set_target_properties(Editor PROPERTIES LINKER_LANGUAGE CXX)
		if (WIN32)
			set_visual_studio_debugger_command(Editor "$${CRYENGINE_DIR}/bin/win_x64/Sandbox.exe" "-project \\"$${PROJECT_DIR}$project_file_name\\"")
		endif()
	endif()
else()
	add_library(GameLauncher STATIC "$${CRYENGINE_DIR}/Code/CryEngine/CryCommon/CryCore/Platform/platform.h")
	set_target_properties(GameLauncher PROPERTIES LINKER_LANGUAGE CXX)
	if (WIN32)
		set_visual_studio_debugger_command(GameLauncher "$${CRYENGINE_DIR}/bin/win_x64/GameLauncher.exe" "-project \\"$${PROJECT_DIR}$project_file_name\\"")
	endif()

    add_library(Editor STATIC "$${CRYENGINE_DIR}/Code/CryEngine/CryCommon/CryCore/Platform/platform.h")
    set_target_properties(Editor PROPERTIES LINKER_LANGUAGE CXX)
    if (WIN32)
        set_visual_studio_debugger_command(Editor "$${CRYENGINE_DIR}/bin/win_x64/Sandbox.exe" "-project \\"$${PROJECT_DIR}$project_file_name\\"")
    endif()

	add_library(GameServer STATIC "$${CRYENGINE_DIR}/Code/CryEngine/CryCommon/CryCore/Platform/platform.h")
	set_target_properties(GameServer PROPERTIES LINKER_LANGUAGE CXX)
	if (WIN32)
		set_visual_studio_debugger_command(GameServer "$${CRYENGINE_DIR}/bin/win_x64/Game_Server.exe" "-project \\"$${PROJECT_DIR}$project_file_name\\"")
	endif()
endif()\n''')
    cmake_launcher_projects = launcher_projects.substitute({'project_file_name' : project_file_name})
    return "GameLauncher", cmake_launcher_projects

def add_cpp_sources(directoryname, project_name, code_directory, skip_directories):
    source_count = 0
    sources = ""

    sources += '''add_sources("''' + os.path.basename(directoryname) + '''_uber.cpp"
    PROJECTS ''' + project_name + '''
    SOURCE_GROUP "'''

    relative_path = os.path.relpath(directoryname, code_directory)
    if relative_path != '.':
        sources += relative_path.replace('\\', '\\\\\\\\')
    else:
        sources += "Root"

    sources += '"'

    for filename in glob.iglob(directoryname + "/*.cpp", recursive=False):
        source_count += 1
        sources += '\n\t\t"' + os.path.relpath(filename, code_directory).replace('\\', '/') + '"'

    for filename in glob.iglob(directoryname + "/*.h", recursive=False):
        source_count += 1
        sources += '\n\t\t"' + os.path.relpath(filename, code_directory).replace('\\', '/') + '"'

    sources += "\n)\n"

    for entry in os.scandir(directoryname):
        if os.path.isdir(entry.path) and not os.path.basename(entry.path).startswith(".") and not os.path.exists(os.path.join(entry.path, 'CMakeCache.txt')):
            if os.path.relpath(entry.path, code_directory) in skip_directories:
                continue

            directory_sources = add_cpp_sources(entry.path, project_name, code_directory, [])

            if directory_sources != "":
                source_count += 1
                sources += directory_sources

    if source_count > 0:
        return sources

    return ""

def create_shortcut(file_path, project_file):
    if not has_win_modules:
        return

    if not os.path.exists(file_path):
        return

    project_dir = os.path.dirname(project_file)
    working_dir = os.path.dirname(file_path)
    file_name = os.path.splitext(os.path.basename(file_path))[0]
    shortcut_path = os.path.join(project_dir, file_name) + ".lnk"

    shell = Dispatch('WScript.Shell')
    shortcut = shell.CreateShortCut(shortcut_path)
    shortcut.Targetpath = file_path
    shortcut.WorkingDirectory = working_dir
    shortcut.save()
