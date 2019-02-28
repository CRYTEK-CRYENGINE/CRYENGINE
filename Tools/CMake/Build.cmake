cmake_minimum_required(VERSION 3.6.2)

set(TOOLS_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")

include("${TOOLS_CMAKE_DIR}/Configure.cmake")

if(OPTION_ENGINE OR OPTION_SHADERCACHEGEN OR OPTION_SCALEFORMHELPER OR OPTION_SANDBOX OR OPTION_PAKTOOLS OR OPTION_DOXYGEN_EXAMPLES)
	# Add custom project with just listing of cmake files
	add_subdirectory("${TOOLS_CMAKE_DIR}")

	# Order currently matters for Engine, Games and Launchers
	# 1. CryEngine
	include ("${TOOLS_CMAKE_DIR}/BuildEngine.cmake")
endif()

# Sandbox Editor
if (OPTION_SANDBOX AND OPTION_STATIC_LINKING)
	message(STATUS "Disabling Sandbox - requires dynamic linking")
	set(OPTION_SANDBOX OFF)
endif()

if (OPTION_SANDBOX AND WIN64)
	# Find Qt before including any plugin subdirectories
	if (MSVC_VERSION GREATER 1900) # Visual Studio > 2015
		set(QT_DIR "${SDK_DIR}/Qt/5.6/msvc2015_64/Qt")
	elseif (MSVC_VERSION EQUAL 1900) # Visual Studio 2015
		set(QT_DIR "${SDK_DIR}/Qt/5.6/msvc2015_64/Qt")
	elseif (MSVC_VERSION EQUAL 1800) # Visual Studio 2013
		set(QT_DIR "${SDK_DIR}/Qt/5.6/msvc2013_64")
	elseif (MSVC_VERSION EQUAL 1700) # Visual Studio 2012
		set(QT_DIR "${SDK_DIR}/Qt/5.6/msvc2012_64")
	endif()
	set(Qt5_DIR "${QT_DIR}")

	find_package(Qt5 COMPONENTS Core Gui OpenGL Widgets REQUIRED PATHS "${QT_DIR}" NO_DEFAULT_PATH)

	set(QT_DIR "${QT_DIR}" CACHE INTERNAL "QT directory" FORCE)
	set(Qt5_DIR "${Qt5_DIR}" CACHE INTERNAL "QT directory" FORCE)

	set_property(GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER  "${VS_FOLDER_PREFIX}/Sandbox/AUTOGEN")

	message(STATUS "Include Sandbox Editor")
	include ("${TOOLS_CMAKE_DIR}/BuildSandbox.cmake")
endif()
	
# Only allow building legacy GameDLL's with the engine, assuming that we're not using the project system
if(OPTION_ENGINE AND NOT PROJECT_BUILD)
	# 2. Games
	add_subdirectories_glob("Code/Game*")
endif()
	
# 3. Extensions (deprecated / legacy)
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Code/CryExtensions")
	add_subdirectory(Code/CryExtensions)
endif()

if(OPTION_ENGINE)
	# 4. Plug-ins
	add_subdirectory(Code/CryPlugins)
endif()

# 5. Launchers
include ("${TOOLS_CMAKE_DIR}/BuildLaunchers.cmake")

if (OPTION_CRYMONO)
	add_subdirectory(Code/CryManaged)
endif()

macro(generate_unit_test_targets target_name using_runner_target_name)
	add_custom_target(${target_name})
	set_target_properties(${target_name} PROPERTIES EXCLUDE_FROM_ALL TRUE)
	set_target_properties(${target_name} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD TRUE)
	set_property(TARGET ${target_name} PROPERTY FOLDER "_TEST_")
	get_property(runner TARGET ${using_runner_target_name} PROPERTY OUTPUT_NAME )
	if(NOT runner)
		set(runner ${using_runner_target_name})
	endif()
	#message("WindowsLauncherExe = ${WindowsLauncherExe}")
	file( WRITE "${CMAKE_CURRENT_BINARY_DIR}/${target_name}.vcxproj.user" 
	"<?xml version=\"1.0\" encoding=\"utf-8\"?>
	<Project ToolsVersion=\"14.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">
			<PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64'\">
				<LocalDebuggerCommand>${OUTPUT_DIRECTORY}/${runner}.exe</LocalDebuggerCommand>
				<LocalDebuggerWorkingDirectory>${OUTPUT_DIRECTORY}</LocalDebuggerWorkingDirectory>
				<LocalDebuggerCommandArguments>-run_unit_tests -unit_test_open_failed</LocalDebuggerCommandArguments>
				<DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>
			</PropertyGroup>
			<PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Profile|x64'\">
				<LocalDebuggerCommand>${OUTPUT_DIRECTORY}/${runner}.exe</LocalDebuggerCommand>
				<LocalDebuggerWorkingDirectory>${OUTPUT_DIRECTORY}</LocalDebuggerWorkingDirectory>
				<LocalDebuggerCommandArguments>-run_unit_tests -unit_test_open_failed</LocalDebuggerCommandArguments>
				<DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>
			</PropertyGroup>		
			<PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\">
				<LocalDebuggerCommand>${OUTPUT_DIRECTORY}/${runner}.exe</LocalDebuggerCommand>
				<LocalDebuggerWorkingDirectory>${OUTPUT_DIRECTORY}</LocalDebuggerWorkingDirectory>
				<LocalDebuggerCommandArguments>-run_unit_tests -unit_test_open_failed</LocalDebuggerCommandArguments>
				<DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>
			</PropertyGroup>
			<PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Debug|Win32'\">
				<LocalDebuggerCommand>${OUTPUT_DIRECTORY}/${runner}.exe</LocalDebuggerCommand>
				<LocalDebuggerWorkingDirectory>${OUTPUT_DIRECTORY}</LocalDebuggerWorkingDirectory>
				<LocalDebuggerCommandArguments>-run_unit_tests -unit_test_open_failed</LocalDebuggerCommandArguments>
				<DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>
			</PropertyGroup>
			<PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Profile|Win32'\">
				<LocalDebuggerCommand>${OUTPUT_DIRECTORY}/${runner}.exe</LocalDebuggerCommand>
				<LocalDebuggerWorkingDirectory>${OUTPUT_DIRECTORY}</LocalDebuggerWorkingDirectory>
				<LocalDebuggerCommandArguments>-run_unit_tests -unit_test_open_failed</LocalDebuggerCommandArguments>
				<DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>
			</PropertyGroup>		
			<PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Release|Win32'\">
				<LocalDebuggerCommand>${OUTPUT_DIRECTORY}/${runner}.exe</LocalDebuggerCommand>
				<LocalDebuggerWorkingDirectory>${OUTPUT_DIRECTORY}</LocalDebuggerWorkingDirectory>
				<LocalDebuggerCommandArguments>-run_unit_tests -unit_test_open_failed</LocalDebuggerCommandArguments>
				<DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>
			</PropertyGroup>
		</Project>")

endmacro()

# Run Unit Test
if (OPTION_ENGINE AND (WIN32 OR WIN64))
	generate_unit_test_targets(run_unit_tests WindowsLauncher)
endif()

if (OPTION_SANDBOX AND WIN64)
	generate_unit_test_targets(run_unit_tests_sandbox Sandbox)
endif()

if(WIN64 AND EXISTS "${CRYENGINE_DIR}/Code/Tools/ShaderCacheGen/ShaderCacheGen")
	option(OPTION_SHADERCACHEGEN "Build the shader cache generator." OFF)
endif()

if (OPTION_SHADERCACHEGEN)
	add_subdirectory(Code/Tools/ShaderCacheGen/ShaderCacheGen)
endif()

if (OPTION_PAKTOOLS AND EXISTS "${CRYENGINE_DIR}/Code/Tools/PakEncrypt")
	add_subdirectory(Code/Tools/PakEncrypt)
endif()

if (OPTION_RC AND EXISTS "${CRYENGINE_DIR}/Code/Tools/rc")
	include(ExternalProject)
	ExternalProject_Add(RC
		CMAKE_ARGS "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}" "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}" "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}" "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
		SOURCE_DIR "${CRYENGINE_DIR}/Code/Tools/rc"
		BUILD_COMMAND "${CMAKE_COMMAND}" --build "." --config $<$<CONFIG:Profile>:Release>$<$<NOT:$<CONFIG:Profile>>:$<CONFIG>>
		INSTALL_COMMAND echo "Skipping install"
	)
endif()

if (OPTION_PHYSDBGR)
	add_subdirectory(Code/Tools/PhysDebugger)
endif()


set(CMAKE_INSTALL_MESSAGE LAZY)
install(FILES "${TOOLS_CMAKE_DIR}/modules/CryCommonConfig.cmake" DESTINATION share/cmake)
install(FILES "${TOOLS_CMAKE_DIR}/modules/CryActionConfig.cmake" DESTINATION share/cmake)

copy_binary_files_to_target()
