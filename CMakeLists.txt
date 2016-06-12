cmake_minimum_required(VERSION 3.1)

enable_language(C)
enable_language(CXX)


set_property(GLOBAL PROPERTY DEBUG_CONFIGURATIONS Debug Profile)

if (${CMAKE_GENERATOR} MATCHES Durango)
	include(${CMAKE_SOURCE_DIR}/Tools/CMake/MSVC-Durango.cmake)
endif ()

if (${CMAKE_GENERATOR} MATCHES ORBIS)
	include(${CMAKE_SOURCE_DIR}/Tools/CMake/MSVC-Orbis.cmake)
endif()

if(WIN32)
	set(CMAKE_SYSTEM_VERSION 10.0.10586.0)
	include(${CMAKE_SOURCE_DIR}/Tools/CMake/MSVC-Windows.cmake)
endif()

if(NOT MSVC)
	set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -std=c++11)
endif()

if(NOT ${CMAKE_GENERATOR} MATCHES "Visual Studio")
	set(valid_configs Debug Profile Release)
	list(FIND valid_configs "${CMAKE_BUILD_TYPE}" config_index)
	if(${config_index} EQUAL -1)
		message(SEND_ERROR "Build type \"${CMAKE_BUILD_TYPE}\" is not supported, set CMAKE_BUILD_TYPE to one of ${valid_configs}")
	endif()
endif()

#if (${BUILD_PLATFORM} MATCHES ANDROID)
#	include(${CMAKE_SOURCE_DIR}/Tools/CMake/Android.cmake)
#endif()

include(${CMAKE_SOURCE_DIR}/Tools/CMake/CopyFilesToBin.cmake)

#set(ANDROID TRUE)

if(ANDROID)
	unset(WIN32)
	unset(WIN64)
	set(OPTION_STATIC_LINKING TRUE)
	SET(CMAKE_SYSTEM_NAME Android)
	include(${CMAKE_SOURCE_DIR}/Tools/CMake/Android.cmake)	

endif(ANDROID)

project(CryEngine_CMake_${BUILD_PLATFORM} CXX C)

set(INTERMEDIATE_DIRECTORY "${CMAKE_SOURCE_DIR}/BinTemp")

# Bootstrap support
include(${CMAKE_SOURCE_DIR}/Tools/CMake/Bootstrap.cmake)

if (${CMAKE_GENERATOR} MATCHES Durango OR ${CMAKE_GENERATOR} MATCHES ORBIS)
	# WIN32 Should be unset  again after project( line to work correctly
	unset(WIN32)
endif ()

if(WIN32)
	set(WINDOWS TRUE)
endif()

if(LINUX32 OR LINUX64)
	set(LINUX TRUE)
endif()

# Define Options
option(OPTION_PROFILE "Enable Profiling" ON)
option(OPTION_UNITY_BUILD "Enable Unity Build" ON)
option(OPTION_ENABLE_BROFILER "Enable Brofiler profiler support" ON)
option(OPTION_ENABLE_SDK_OCULUS "Enable Oculus SDK" ON)

# Select Game Project
set(OPTION_GAME_PROJECT "GameSDK" CACHE STRING "What Game project to use")
set(OPTION_GAME_PROJECT_VALUES "GameSDK" "GameMono" "GameZero")
set_property(CACHE OPTION_GAME_PROJECT PROPERTY STRINGS ${OPTION_GAME_PROJECT_VALUES})
list(FIND OPTION_GAME_PROJECT_VALUES ${OPTION_GAME_PROJECT} OPTION_GAME_PROJECT_INDEX)
if(${OPTION_GAME_PROJECT_INDEX} EQUAL -1)
	message(FATAL_ERROR "Option ${OPTION_GAME_PROJECT} is not supported, valid entries are ${OPTION_GAME_PROJECT_VALUES}")
endif()


if(WIN64 AND EXISTS "${CMAKE_SOURCE_DIR}/Code/Sandbox")
	set(OPTION_SANDBOX "No" CACHE STRING "Enable Sandbox")
	set_property(CACHE OPTION_SANDBOX PROPERTY STRINGS "No" "Qt" "Legacy")
endif()

option(OPTION_SOUND_WWISE "Enable WWise Sound System" ON)
option(OPTION_PCH "Enable Precompiled Headers" OFF)
option(OPTION_CRY_EXTENSIONS "Enable CryExtensions" ON)

# Print current project settings
MESSAGE("OPTION_GAME_PROJECT = ${OPTION_GAME_PROJECT}")
MESSAGE("CMAKE_SYSTEM_NAME = ${CMAKE_SYSTEM_NAME}")
MESSAGE("CMAKE_GENERATOR = ${CMAKE_GENERATOR}")
MESSAGE("CMAKE_CONFIGURATION_TYPES = ${CMAKE_CONFIGURATION_TYPES}")
MESSAGE("BUILD_PLATFORM = ${BUILD_PLATFORM}")
MESSAGE("OPTION_PROFILE = ${OPTION_PROFILE}")
MESSAGE("OPTION_SOUND_WWISE = ${OPTION_SOUND_WWISE}")
MESSAGE("OPTION_PCH = ${OPTION_PCH}")

if (OPTION_PROFILE)
	set_property( DIRECTORY PROPERTY COMPILE_DEFINITIONS $<$<CONFIG:Release>:_PROFILE> )
else()
	set_property( DIRECTORY PROPERTY COMPILE_DEFINITIONS $<$<CONFIG:Release>:_RELEASE> )
endif()

if(OPTION_UNITY_BUILD)
	message("UNITY BUILD Enabled")
endif()

if(ORBIS)
	set(OPTION_STATIC_LINKING TRUE)
else()
	option(OPTION_STATIC_LINKING "Link all CryEngine modules as static libs to single exe" OFF)
endif()

if (MSVC_VERSION EQUAL 1800 OR MSVC_VERSION EQUAL 1900)       # Visual Studio 2013/2015
	#message( "Visual Studio 2013/2015 uses v110 toolset (Visual Studio 2012 compiler)" )
	# For Visual Studio 2013 we use Visual Studio 2012 compiler
	#set(CMAKE_VS_PLATFORM_TOOLSET "v110" CACHE STRING "Visual Studio Platform Toolset" FORCE)
	#set(CMAKE_GENERATOR_TOOLSET "v110" CACHE STRING "Visual Studio Platform Toolset" FORCE)
endif()

if (MSVC_VERSION EQUAL 1900) # Visual Studio 2015
	set(MSVC_LIB_PREFIX vc140)
	set(QT_DIR ${CMAKE_SOURCE_DIR}/Code/SDKs/Qt/5.6/msvc2015_64)
elseif (MSVC_VERSION EQUAL 1800) # Visual Studio 2013
	set(MSVC_LIB_PREFIX vc120)
	set(QT_DIR ${CMAKE_SOURCE_DIR}/Code/SDKs/Qt/5.6/msvc2013_64)
elseif (MSVC_VERSION EQUAL 1700) # Visual Studio 2012
	set(MSVC_LIB_PREFIX "vc110")
	set(QT_DIR ${CMAKE_SOURCE_DIR}/Code/SDKs/Qt/5.6/msvc2012_64)
else()
	set(MSVC_LIB_PREFIX "")
endif()
set(Qt5_DIR ${QT_DIR})

# SDK Directory
set(SDK_DIR ${CMAKE_SOURCE_DIR}/Code/SDKs)
set(CRY_LIBS_DIR ${CMAKE_SOURCE_DIR}/Code/Libs)
#set(DXSDK_DIR ${CMAKE_SOURCE_DIR}/Code/SDKs/DXSDK)
set(WINSDK_SDK_DIR "${CMAKE_SOURCE_DIR}/Code/SDKs/Microsoft Windows SDK")
set(WINSDK_SDK_LIB_DIR "${WINSDK_SDK_DIR}/V8.0/Lib/Win8/um")
set(WINSDK_SDK_INCLUDE_DIR "${WINSDK_SDK_DIR}/V8.0/Include/um")

include(${CMAKE_SOURCE_DIR}/Tools/CMake/modules/SDL2.cmake)

# custom defines
set(global_defines "CRYENGINE_DEFINE")

if(OPTION_STATIC_LINKING)
	# Enable static libraries
	MESSAGE( "Use Static Linking (.lib)" )
	set(BUILD_SHARED_LIBS FALSE)
else()
	# Enable dynamic libraries
	MESSAGE( "Use Dynamic Linking (.dll)" )
	set(BUILD_SHARED_LIBS TRUE)
endif()

if (OUTPUT_DIRECTORY)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIRECTORY}")
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIRECTORY}")
	set(EXECUTABLE_OUTPUT_PATH "${OUTPUT_DIRECTORY}")
endif (OUTPUT_DIRECTORY)


include(${CMAKE_SOURCE_DIR}/Tools/CMake/CommonMacros.cmake)

include_directories( ${SDK_DIR} )
include_directories( ${CMAKE_SOURCE_DIR}/Code/CryEngine/CryCommon )

set( BOOST_DIR ${SDK_DIR}/boost )
include_directories( ${BOOST_DIR} )

if(WIN32)
	# Common Libriries linked to all targets
	set(COMMON_LIBS jitprofiling libittnotify Ntdll User32 Advapi32 Ntdll Ws2_32)

	include_directories( ${SDK_DIR}/GPA/include )
	if (WIN64)
		link_directories( ${SDK_DIR}/GPA/lib64 )
	else()
		link_directories( ${SDK_DIR}/GPA/lib32 )
	endif()
else()
	# Common Libriries linked to all targets
	set(COMMON_LIBS )
endif()

# Serialization library
include_directories( ${CRY_LIBS_DIR}/yasli )
include_directories( ${SDK_DIR}/yasli )

# add global defines
foreach( current_define ${platform_defines} )
	add_definitions(-D${current_define})
endforeach()

if (OPTION_ENABLE_BROFILER)
	add_definitions(-DUSE_BROFILER)
	include_directories( ${SDK_DIR}/Brofiler )
	link_directories( ${SDK_DIR}/Brofiler )
	if (WIN32 AND NOT WIN64)
		set(COMMON_LIBS ${COMMON_LIBS} ProfilerCore32)
	else()
		set(COMMON_LIBS ${COMMON_LIBS} ProfilerCore64)
	endif()
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
if (OPTION_GAME_PROJECT STREQUAL "BenchmarkVR")
	add_subdirectory(Code/Game_Skyharbor/BenchmarkFramework)
	add_subdirectory(Code/Game_Skyharbor/GameDll)
	set_solution_folder("Projects" BenchmarkFramework)
	set_solution_folder("Projects" CryGameSkyharbor)
endif()

# CryEngine
add_subdirectory(Code/CryEngine/Cry3DEngine)
add_subdirectory(Code/CryEngine/Cry3DEngine/ParticleSystem)
add_subdirectory(Code/CryEngine/CryAction)
add_subdirectory(Code/CryEngine/CryAISystem)
add_subdirectory(Code/CryEngine/CryAnimation)
add_subdirectory(Code/CryEngine/CryDynamicResponseSystem)
add_subdirectory(Code/CryEngine/CryEntitySystem)
add_subdirectory(Code/CryEngine/CryFont)
add_subdirectory(Code/CryEngine/CryInput)
add_subdirectory(Code/CryEngine/CryMovie)
add_subdirectory(Code/CryEngine/CryNetwork)
add_subdirectory(Code/CryEngine/CryPhysics)
add_subdirectory(Code/CryEngine/RenderDll/XRenderD3D9)
add_subdirectory(Code/CryEngine/CryScriptSystem)
add_subdirectory(Code/CryEngine/CryAudioSystem)
add_subdirectory(Code/CryEngine/CryAudioSystem/Common)
add_subdirectory(Code/CryEngine/CryAudioSystem/implementations/CryAudioImplFmod)
add_subdirectory(Code/CryEngine/CryAudioSystem/implementations/CryAudioImplSDLMixer)
if (OPTION_SOUND_WWISE)
	add_subdirectory(Code/CryEngine/CryAudioSystem/implementations/CryAudioImplWwise)
endif()
add_subdirectory(Code/CryEngine/CrySystem)

# CryCommon headers only project
add_subdirectory(Code/CryEngine/CryCommon)

# Shaders custom project
add_subdirectory(Engine/Shaders)

if (WIN32)
	add_subdirectory(Code/CryEngine/CryLiveCreate)
	# CryExtensions
	#add_subdirectory(Code/CryExtensions/GameStream)
endif (WIN32)

if (OPTION_CRY_EXTENSIONS)
	# CryExtensions
	#add_subdirectory(Code/CryExtensions/CryDescriptor)
	#add_subdirectory(Code/CryExtensions/CryDrsCommonElements/Module)
	add_subdirectory(Code/CryExtensions/CryLobby/Module)
	#add_subdirectory(Code/CryExtensions/CrySchematyc)
endif()

# Game
if (OPTION_GAME_PROJECT STREQUAL "GameSDK")
	add_subdirectory(Code/GameSDK/GameDll)
endif()

if (OPTION_GAME_PROJECT STREQUAL "GameMono")
	add_subdirectory(Code/GameMono/GameDll)
endif()

if (OPTION_GAME_PROJECT STREQUAL "GameZero")
	add_subdirectory(Code/GameZero/GameDll)
endif()

# Launchers
if (DURANGO)
	MESSAGE( "DURANGO LAUNCHER" )
	add_subdirectory(Code/Launcher/DurangoLauncher)
elseif (ORBIS)
	add_subdirectory(Code/Launcher/OrbisLauncher)
elseif (WIN32)
	add_subdirectory(Code/Launcher/DedicatedLauncher)
	add_subdirectory(Code/Launcher/WindowsLauncher)
endif ()

#Libs
add_subdirectory(Code/Libs/freetype)
add_subdirectory(Code/Libs/expat)
add_subdirectory(Code/Libs/lua)
add_subdirectory(Code/Libs/zlib)
add_subdirectory(Code/Libs/lz4)	
add_subdirectory(Code/Libs/md5)
add_subdirectory(Code/Libs/tiff)
add_subdirectory(Code/Libs/lzma)
add_subdirectory(Code/Libs/lzss)
add_subdirectory(Code/Libs/bigdigits)
add_subdirectory(Code/Libs/strophe)
add_subdirectory(Code/Libs/yasli)
add_subdirectory(Code/Libs/tomcrypt)
add_subdirectory(Code/Libs/prt)
add_subdirectory(Code/Libs/python)

if (OPTION_ENABLE_SDK_OCULUS)
	add_subdirectory(Code/Libs/oculus)
elseif()
	add_definitions(-DEXCLUDE_OCULUS_SDK)
endif()

# Sandbox Editor
if (OPTION_SANDBOX AND WIN64)
	MESSAGE("Include Sandbox Editor")
	find_package(Qt5 COMPONENTS Core Gui OpenGL Widgets REQUIRED PATHS "${QT_DIR}")
	set(CMAKE_INCLUDE_CURRENT_DIR ON)
	set(CMAKE_AUTOMOC_RELAXED_MODE TRUE)
	set(CMAKE_AUTOMOC ON)
	set(CMAKE_AUTORCC ON)
	set_property(GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER  "AUTOGEN")
	set_property(GLOBAL PROPERTY AUTOMOC_TARGETS_FOLDER  "AUTOMOC")	

	if (OPTION_SANDBOX STREQUAL "Qt")
		set(EDITOR_DIR "${CMAKE_SOURCE_DIR}/Code/Sandbox/EditorQt" )
		add_subdirectory(Code/Sandbox/EditorQt)
		add_subdirectory(Code/Sandbox/Plugins/3DConnexionPlugin)	
		add_subdirectory(Code/Sandbox/Plugins/EditorConsole)
	elseif(OPTION_SANDBOX STREQUAL "Legacy")
		set(EDITOR_DIR "${CMAKE_SOURCE_DIR}/Code/Sandbox/Editor" )
		add_subdirectory(Code/Sandbox/Editor)
	endif()

	add_subdirectory(Code/Sandbox/Plugins/EditorCommon)

	add_subdirectory(Code/Sandbox/Plugins/CryDesigner)	
	add_subdirectory(Code/Sandbox/Plugins/EditorAudioControlsEditor)
	add_subdirectory(Code/Sandbox/Plugins/EditorAudioControlsEditor/common)
	add_subdirectory(Code/Sandbox/Plugins/EditorAudioControlsEditor/EditorFmod)
	add_subdirectory(Code/Sandbox/Plugins/EditorAudioControlsEditor/EditorSDLMixer)
	if (OPTION_SOUND_WWISE)
		add_subdirectory(Code/Sandbox/Plugins/EditorAudioControlsEditor/EditorWwise)
	endif()	
	add_subdirectory(Code/Sandbox/Plugins/EditorAnimation)
	add_subdirectory(Code/Sandbox/Plugins/EditorDynamicResponseSystem)
	add_subdirectory(Code/Sandbox/Plugins/EditorEnvironment)
	add_subdirectory(Code/Sandbox/Plugins/EditorParticle)
	add_subdirectory(Code/Sandbox/Plugins/EditorTrackView)
	add_subdirectory(Code/Sandbox/Plugins/FbxPlugin)
	add_subdirectory(Code/Sandbox/Plugins/MeshImporter)
endif()

copy_binary_files_to_target()
