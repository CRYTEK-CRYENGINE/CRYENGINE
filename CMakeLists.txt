cmake_minimum_required(VERSION 2.8.11)

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

#if (${BUILD_PLATFORM} MATCHES ANDROID)
#	include(${CMAKE_SOURCE_DIR}/Tools/CMake/Android.cmake)
#endif()

include(${CMAKE_SOURCE_DIR}/Tools/CMake/CopyFilesToBin.cmake)

#set(ANDROID TRUE)

if(ANDROID)
	unset(WIN32)
	set(OPTION_STATIC_LINKING TRUE)
	SET(CMAKE_SYSTEM_NAME Android)
	include(${CMAKE_SOURCE_DIR}/Tools/CMake/Android.cmake)	

endif(ANDROID)

project(CryEngine_CMake_${BUILD_PLATFORM} CXX C)

set(INTERMEDIATE_DIRECTORY "${CMAKE_SOURCE_DIR}/BinTemp")

if (${CMAKE_GENERATOR} MATCHES Durango OR ${CMAKE_GENERATOR} MATCHES ORBIS)
	# WIN32 Should be unset  again after project( line to work correctly
	unset(WIN32)
endif ()

# Define Options
option(OPTION_PROFILE "Enable Profiling" ON)
option(OPTION_UNITY_BUILD "Enable Unity Build" OFF)
option(OPTION_ENABLE_BROFILER "Enable Brofiler profiler support" ON)
option(OPTION_ENABLE_SDK_OCULUS "Enable Oculus SDK" ON)

# Select Game Project
set(OPTION_GAME_PROJECT "GameSDK" CACHE STRING "What Game project to use")
set(OPTION_GAME_PROJECT_VALUES "GameSDK" "BenchmarkVR" "Hunt" "NoGame")
set_property(CACHE OPTION_GAME_PROJECT PROPERTY STRINGS ${OPTION_GAME_PROJECT_VALUES})
list(FIND OPTION_GAME_PROJECT_VALUES ${OPTION_GAME_PROJECT} OPTION_GAME_PROJECT_INDEX)
if(${OPTION_GAME_PROJECT_INDEX} EQUAL -1)
	message(FATAL_ERROR "Option ${OPTION_GAME_PROJECT} is not supported, valid entries are ${OPTION_GAME_PROJECT_VALUES}")
endif()



if(WIN64)
	option(OPTION_SANDBOX "Enable Sandbox" ON)
endif()

option(OPTION_SOUND_WWISE "Enable WWise Sound System" ON)
option(OPTION_PCH "Enable Precompiled Headers" ON)
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
	set(QT_DIR ${CMAKE_SOURCE_DIR}/Code/SDKs/Qt/5.5/msvc2015_64)
elseif (MSVC_VERSION EQUAL 1800) # Visual Studio 2013
	set(MSVC_LIB_PREFIX vc120)
	set(QT_DIR ${CMAKE_SOURCE_DIR}/Code/SDKs/Qt/5.5/msvc2013_64)
elseif (MSVC_VERSION EQUAL 1700) # Visual Studio 2012
	set(MSVC_LIB_PREFIX "vc110")
	set(QT_DIR ${CMAKE_SOURCE_DIR}/Code/SDKs/Qt/5.5/msvc2012_64)
else()
	set(MSVC_LIB_PREFIX "")
endif()

# SDK Directory
set(SDK_DIR ${CMAKE_SOURCE_DIR}/Code/SDKs)
set(CRY_LIBS_DIR ${CMAKE_SOURCE_DIR}/Code/Libs)
#set(DXSDK_DIR ${CMAKE_SOURCE_DIR}/Code/SDKs/DXSDK)
set(WINSDK_SDK_DIR "${CMAKE_SOURCE_DIR}/Code/SDKs/Microsoft Windows SDK")
set(WINSDK_SDK_LIB_DIR "${WINSDK_SDK_DIR}/V8.0/Lib/Win8/um")
set(WINSDK_SDK_INCLUDE_DIR "${WINSDK_SDK_DIR}/V8.0/Include/um")

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
	set(COMMON_LIBS jitprofiling libittnotify Ntdll)

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
	set(COMMON_LIBS ${COMMON_LIBS} ProfilerCore64)
endif()


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
add_subdirectory(Code/CryEngine/CryEntitySystem)
add_subdirectory(Code/CryEngine/CryFont)
add_subdirectory(Code/CryEngine/CryInput)
add_subdirectory(Code/CryEngine/CryMovie)
add_subdirectory(Code/CryEngine/CryNetwork)
add_subdirectory(Code/CryEngine/CryNetwork/Lobby)
add_subdirectory(Code/CryEngine/CryPhysics)
add_subdirectory(Code/CryEngine/RenderDll/XRenderD3D9)

#add_subdirectory(Code/CryEngine/RenderDll_LG/XRenderD3D9)
#add_subdirectory(Code/CryEngine/RenderDll_LG/XRenderD3D9)
add_subdirectory(Code/CryEngine/CryScriptSystem)
add_subdirectory(Code/CryEngine/CryAudioSystem)
#add_subdirectory(Code/CryEngine/CryAudioCommon)
add_subdirectory(Code/CryEngine/CryAudioSystem/implementations/CryAudioImplSDLMixer)
add_subdirectory(Code/CryEngine/CryAudioSystem/implementations/CryAudioImplWwise)
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
	add_subdirectory(Code/CryExtensions/CrySchematyc)
	#add_subdirectory(Code/CryExtensions/CryDescriptor)
endif()

# Game
#add_subdirectory(Code/Game_Hunt/GameDll)
#add_subdirectory(Code/Game_Hunt/CryGameScripts)
if (OPTION_GAME_PROJECT STREQUAL "GameSDK")
	add_subdirectory(Code/GameSDK/GameDll)
endif()

if (OPTION_GAME_PROJECT STREQUAL "Hunt")
	add_subdirectory(Code/Game_Hunt/GameDll)
	add_subdirectory(Code/Game_Hunt/UIFramework)
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

	set(EDITOR_DIR ${CMAKE_SOURCE_DIR}/Code/Sandbox/EditorQt )
	#add_subdirectory(Code/Sandbox/Editor)
	add_subdirectory(Code/Sandbox/EditorQt)
	add_subdirectory(Code/Sandbox/Plugins/EditorCommon)
	add_subdirectory(Code/Sandbox/Plugins/EditorAudioControlsEditor)
	add_subdirectory(Code/Sandbox/Plugins/EditorAudioControlsEditor/common)
	add_subdirectory(Code/Sandbox/Plugins/EditorAudioControlsEditor/EditorSDLMixer)
	add_subdirectory(Code/Sandbox/Plugins/EditorAudioControlsEditor/EditorWwise)
	
	add_subdirectory(Code/Sandbox/Plugins/EditorAnimation)
	#add_subdirectory(Code/Sandbox/Plugins/EditorConsole)
	add_subdirectory(Code/Sandbox/Plugins/CryDesigner)
if (MSVC_VERSION LESS 1900)
	add_subdirectory(Code/Sandbox/Plugins/FbxPlugin)
	add_subdirectory(Code/Sandbox/Plugins/PerforcePlugin)
endif()
	
	set_solution_folder("Sandbox" Editor)
	set_solution_folder("Sandbox" EditorQt)
	set_solution_folder("Sandbox/Plugins" EditorCommon)
	set_solution_folder("Sandbox/Plugins" EditorAnimation)
	set_solution_folder("Sandbox/Plugins" EditorConsole)
	set_solution_folder("Sandbox/Plugins" CryDesigner)
	set_solution_folder("Sandbox/Plugins" EditorFbxPlugin)
	set_solution_folder("Sandbox/Plugins" EditorFbxImport)
	set_solution_folder("Sandbox/Plugins" PerforcePlugin)
	
	set_solution_folder("Sandbox/Plugins/Audio" EditorAudioControlsEditor)
	set_solution_folder("Sandbox/Plugins/Audio" EditorAudioControlsEditorCommon)
	set_solution_folder("Sandbox/Plugins/Audio" EditorCryAudioImplSDLMixer)
	set_solution_folder("Sandbox/Plugins/Audio" EditorCryAudioImplWwise)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_solution_folder("CryCommon" CryCommon)

set_solution_folder("CryEngine/Shaders" Shaders)
set_solution_folder("CryEngine/Audio" CryAudioSystem)
set_solution_folder("CryEngine/Audio/Implementations" CryAudioImplFmod)
set_solution_folder("CryEngine/Audio/Implementations" CryAudioImplWwise)
set_solution_folder("CryEngine/Audio/Implementations" CryAudioImplSDLMixer)
set_solution_folder("CryEngine" Cry3DEngine)
set_solution_folder("CryEngine" CryParticleSystem)
set_solution_folder("CryEngine" CryAISystem)
set_solution_folder("CryEngine" CryAnimation)
set_solution_folder("CryEngine" CryEntitySystem)
set_solution_folder("CryEngine" CryFont)
set_solution_folder("CryEngine" CryInput)
set_solution_folder("CryEngine" CryLiveCreate)
set_solution_folder("CryEngine" CryMovie)
set_solution_folder("CryEngine" CryNetwork)
set_solution_folder("CryEngine" CryLobby)
set_solution_folder("CryEngine" CryOnline)
set_solution_folder("CryEngine" CryPhysics)
set_solution_folder("CryEngine" CryPlatform)
set_solution_folder("CryEngine" CryScriptSystem)
set_solution_folder("CryEngine" CrySystem)
set_solution_folder("CryEngine" CryRenderD3D11)
set_solution_folder("CryEngine" CryRenderD3D12)

set_solution_folder("CryExtensions" GameStream)

set_solution_folder("Projects" CryAction)
set_solution_folder("Projects" CryGameHomefront2)

set_solution_folder("Projects" CryGameSDK)
set_solution_folder("Projects" GameHunt)
set_solution_folder("Projects" DescriptorFramework)
set_solution_folder("Projects" Schematyc)
set_solution_folder("Projects" UIFramework)

set_solution_folder("Projects" CryGameScripts)

set_solution_folder("Launchers" DedicatedArbitratorLauncher)
set_solution_folder("Launchers" DedicatedLauncher)
set_solution_folder("Launchers" WindowsLauncher)
set_solution_folder("Launchers" DurangoLauncher)
set_solution_folder("Launchers" OrbisLauncher)

set_solution_folder("Libs" expat)
set_solution_folder("Libs" freetype)
set_solution_folder("Libs" lua)
set_solution_folder("Libs" zlib)
set_solution_folder("Libs" lz4)
set_solution_folder("Libs" lzma)
set_solution_folder("Libs" lzss)
set_solution_folder("Libs" md5)
set_solution_folder("Libs" tiff)
set_solution_folder("Libs" bigdigits)
set_solution_folder("Libs" oculus)
set_solution_folder("Libs" strophe)
set_solution_folder("Libs" yasli)
set_solution_folder("Libs" tomcrypt)
set_solution_folder("Libs" prt)
set_solution_folder("Libs" python27)

copy_binary_files_to_target()
