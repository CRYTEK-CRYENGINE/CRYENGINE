if(DURANGO OR ORBIS OR ANDROID OR LINUX)
	unset(WIN32)
	unset(WINDOWS)
elseif(WIN32) # Either Win32 or Win64
	include("${CMAKE_CURRENT_LIST_DIR}/toolchain/windows/WindowsPC-MSVC.cmake")
endif()

message(STATUS "CMAKE_SYSTEM = ${CMAKE_SYSTEM}")
message(STATUS "CMAKE_SYSTEM_NAME = ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_SYSTEM_VERSION = ${CMAKE_SYSTEM_VERSION}")
message(STATUS "BUILD_PLATFORM = ${BUILD_PLATFORM}")
message(STATUS "BUILD_CPU_ARCHITECTURE = ${BUILD_CPU_ARCHITECTURE}")
message(STATUS "OUTPUT_DIRECTORY_NAME = ${OUTPUT_DIRECTORY_NAME}")

# C/C++ languages required.
enable_language(C)
enable_language(CXX)

if(NOT ${CMAKE_GENERATOR} MATCHES "Visual Studio")
	set(valid_configs Debug Profile Release)
	list(FIND valid_configs "${CMAKE_BUILD_TYPE}" config_index)
	if(${config_index} EQUAL -1)
		message(SEND_ERROR "Build type \"${CMAKE_BUILD_TYPE}\" is not supported, set CMAKE_BUILD_TYPE to one of ${valid_configs}")
	endif()
endif()

# Correct output directory slashes, has to be done after toolchain includes
if (OUTPUT_DIRECTORY)
	string(REPLACE "\\" "/" OUTPUT_DIRECTORY "${OUTPUT_DIRECTORY}")
else()
	# Note that PROJECT_DIR can be different depending on whether we're building the engine or a standalone game / plugin project
	set(OUTPUT_DIRECTORY "${PROJECT_DIR}/bin")
endif()

if(OUTPUT_DIRECTORY_NAME)
	set(OUTPUT_DIRECTORY "${OUTPUT_DIRECTORY}/${OUTPUT_DIRECTORY_NAME}")
	set(OUTPUT_DIRECTORY_NAME "")
endif()

set(BASE_OUTPUT_DIRECTORY         "${OUTPUT_DIRECTORY}")
set(BASE_OUTPUT_DIRECTORY_DEBUG   "${OUTPUT_DIRECTORY}")
set(BASE_OUTPUT_DIRECTORY_PROFILE "${OUTPUT_DIRECTORY}")
set(BASE_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIRECTORY}_release")

set(OUTPUT_DIRECTORY_SUFFIX "" CACHE STRING "Optional suffix for the binary output directory")
if(OUTPUT_DIRECTORY_SUFFIX)
	set(OUTPUT_DIRECTORY "${OUTPUT_DIRECTORY}_${OUTPUT_DIRECTORY_SUFFIX}")
endif()

set(METADATA_PROJECT_NAME "CryEngine" CACHE STRING "Name of the solution project")
project("${METADATA_PROJECT_NAME}_CMake_${BUILD_PLATFORM}" CXX C)

# Prefix all Visual Studio solution folder names with this string
set( VS_FOLDER_PREFIX "CRYENGINE/" )

set(MODULES CACHE INTERNAL "List of engine modules being built" FORCE)
set(GAME_MODULES CACHE INTERNAL "List of game modules being built" FORCE)

if (DURANGO OR ORBIS OR ANDROID OR LINUX)
	# WIN32 Should be unset  again after project( line to work correctly
	unset(WIN32)
	unset(WINDOWS)
endif ()

set(game_folder CACHE INTERNAL "Game folder used for resource files on Windows" FORCE)

# Define Options
get_property(global_defines DIRECTORY "${CRYENGINE_DIR}" PROPERTY COMPILE_DEFINITIONS)
get_property(global_includes DIRECTORY "${CRYENGINE_DIR}" PROPERTY INCLUDE_DIRECTORIES)
get_property(global_links DIRECTORY "${CRYENGINE_DIR}" PROPERTY LINK_DIRECTORIES)

if (OPTION_PROFILE)
	set_property( DIRECTORY PROPERTY COMPILE_DEFINITIONS $<$<CONFIG:Release>:_PROFILE> )
else()
	set_property( DIRECTORY PROPERTY COMPILE_DEFINITIONS $<$<CONFIG:Release>:_RELEASE> )
endif()

if(OPTION_UNITY_BUILD)
	message(STATUS "UNITY BUILD Enabled")
endif()

set(CRY_LIBS_DIR "${CRYENGINE_DIR}/Code/Libs")
set(CRY_EXTENSIONS_DIR "${CRYENGINE_DIR}/Code/CryExtensions")

set(WINSDK_SDK_DIR "${SDK_DIR}/Microsoft Windows SDK")
set(WINSDK_SDK_LIB_DIR "${WINSDK_SDK_DIR}/V8.0/Lib/Win8/um")
set(WINSDK_SDK_INCLUDE_DIR "${WINSDK_SDK_DIR}/V8.0/Include/um")

if (NOT EXISTS "${CMAKE_BINARY_DIR}/ProjectCVarOverrides.h")
	file(WRITE "${CMAKE_BINARY_DIR}/ProjectCVarOverrides.h" "")
endif ()
list(APPEND global_defines "CRY_CVAR_OVERRIDE_FILE=\"${CMAKE_BINARY_DIR}/ProjectCVarOverrides.h\"")

if (NOT EXISTS "${CMAKE_BINARY_DIR}/ProjectCVarWhitelist.h")
	file(WRITE "${CMAKE_BINARY_DIR}/ProjectCVarWhitelist.h" "")
endif ()
list(APPEND global_defines "CRY_CVAR_WHITELIST_FILE=\"${CMAKE_BINARY_DIR}/ProjectCVarWhitelist.h\"")

if (NOT EXISTS "${CMAKE_BINARY_DIR}/ProjectEngineDefineOverrides.h")
	file(WRITE "${CMAKE_BINARY_DIR}/ProjectEngineDefineOverrides.h" "")
endif ()
list(APPEND global_defines "CRY_ENGINE_DEFINE_OVERRIDE_FILE=\"${CMAKE_BINARY_DIR}/ProjectEngineDefineOverrides.h\"")

if (OPTION_RUNTIME_CVAR_OVERRIDES)
	list(APPEND global_defines "USE_RUNTIME_CVAR_OVERRIDES")
endif()

# custom defines
list(APPEND global_defines "CRYENGINE_DEFINE")

include("${TOOLS_CMAKE_DIR}/CommonOptions.cmake")

# Must be included after SDK_DIR definition
include("${TOOLS_CMAKE_DIR}/CopyFilesToBin.cmake")

if(MSVC_VERSION AND NOT OPTION_PGO STREQUAL "Off")
	if (OPTION_RECODE)
		MESSAGE(STATUS "Cannot support Recode with PGO enabled - disabling Recode support")
		set(OPTION_RECODE OFF CACHE BOOL "Enable support for Recode" FORCE)
	endif()
	if (NOT OPTION_LTCG)
		MESSAGE(STATUS "OPTION_LTCG is required for PGO - enabling LTCG")
		set(OPTION_LTCG ON CACHE BOOL "Enable link-time code generation/optimization" FORCE)
	endif()
endif()


if (OPTION_STATIC_LINKING_WITH_GAME_AS_DLL)
		MESSAGE(STATUS "Enabling OPTION_STATIC_LINKING because OPTION_STATIC_LINKING_WITH_GAME_AS_DLL was set")
		set(OPTION_STATIC_LINKING ON CACHE BOOL "Enabling OPTION_STATIC_LINKING because OPTION_STATIC_LINKING_WITH_GAME_AS_DLL was set" FORCE)
endif()

if(OPTION_STATIC_LINKING)
	# Enable static libraries
	MESSAGE(STATUS "Use Static Linking (.lib/.a)" )
	set(BUILD_SHARED_LIBS FALSE)
else()
	# Enable dynamic librariesboost
	MESSAGE(STATUS "Use Dynamic Linking (.dll/.so)" )
	set(BUILD_SHARED_LIBS TRUE)
endif()

if (OUTPUT_DIRECTORY)
	if(OPTION_DEDICATED_SERVER)
		set(OUTPUT_DIRECTORY "${OUTPUT_DIRECTORY}_dedicated")
	endif()

	message(STATUS "OUTPUT_DIRECTORY=${OUTPUT_DIRECTORY}")
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIRECTORY}")
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIRECTORY}")
	set(EXECUTABLE_OUTPUT_PATH "${OUTPUT_DIRECTORY}")

	# Make sure the output directory exists
	file(MAKE_DIRECTORY "${OUTPUT_DIRECTORY}")
endif (OUTPUT_DIRECTORY)

include("${TOOLS_CMAKE_DIR}/ConfigureChecks.cmake")
include("${TOOLS_CMAKE_DIR}/CommonMacros.cmake")
include("${TOOLS_CMAKE_DIR}/Recode.cmake")

if(WINDOWS)
	# Common Libriries linked to all targets
	set(COMMON_LIBS Ntdll User32 Advapi32 Ntdll Ws2_32)

	if (EXISTS "${SDK_DIR}/GPA" AND OPTION_ENGINE)
		list(APPEND global_includes "${SDK_DIR}/GPA/include" )
		list(APPEND global_links "${SDK_DIR}/GPA/lib64" )
		set(COMMON_LIBS ${COMMON_LIBS} jitprofiling libittnotify)
	endif()
else()
	# Common Libriries linked to all targets
	set(COMMON_LIBS )
endif()

# add global defines
foreach( current_define ${platform_defines} )
	list(APPEND global_defines "${current_define}")
endforeach()

if (OPTION_RELEASE_PROFILING)
	list(APPEND global_defines  "$<$<CONFIG:Release>:PERFORMANCE_BUILD>")
endif()
if (OPTION_RELEASE_LOGGING)
	list(APPEND global_defines  "$<$<CONFIG:Release>:RELEASE_LOGGING>")
endif()
if (OPTION_MEMREPLAY_USES_DETOURS)
	list(APPEND global_defines  MEMREPLAY_USES_DETOURS=1)
else ()
	list(APPEND global_defines  MEMREPLAY_USES_DETOURS=0)
endif ()

if (NOT TARGET WinPixEventRuntime)
	include("${TOOLS_CMAKE_DIR}/modules/WinPixEventRuntime.cmake")
endif()

if (OPTION_ENGINE)
	if(NOT TARGET SDL2)
		include("${TOOLS_CMAKE_DIR}/modules/SDL2.cmake")
	endif()

	if(NOT TARGET ncursesw)
		include("${TOOLS_CMAKE_DIR}/modules/ncurses.cmake")
	endif()

	option(OPTION_GEOM_CACHES "Enable Geom Cache" ON)

	if(OPTION_GEOM_CACHES)
		list(APPEND global_defines USE_GEOM_CACHES=1)
	endif()

	option(OPTION_FPE "Enable floating point exceptions" OFF)

	if(OPTION_FPE)
		list(APPEND global_defines USE_FPE=1)
	endif()
endif()

include("${TOOLS_CMAKE_DIR}/modules/Boost.cmake")

# Apply global defines
set_property(DIRECTORY "${CRYENGINE_DIR}" PROPERTY COMPILE_DEFINITIONS ${global_defines})
set_property(DIRECTORY "${CRYENGINE_DIR}" PROPERTY INCLUDE_DIRECTORIES ${global_includes})
set_property(DIRECTORY "${CRYENGINE_DIR}" PROPERTY LINK_DIRECTORIES ${global_links})
# Used by game project when they share the solution with the engine.
set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY COMPILE_DEFINITIONS ${global_defines})
set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY INCLUDE_DIRECTORIES ${global_includes})
set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY LINK_DIRECTORIES ${global_links})

if (MSVC_VERSION GREATER 1900) # Visual Studio > 2015
	set(MSVC_LIB_PREFIX vc140)
elseif (MSVC_VERSION EQUAL 1900) # Visual Studio 2015
	set(MSVC_LIB_PREFIX vc140)
elseif (MSVC_VERSION EQUAL 1800) # Visual Studio 2013
	set(MSVC_LIB_PREFIX vc120)
elseif (MSVC_VERSION EQUAL 1700) # Visual Studio 2012
	set(MSVC_LIB_PREFIX "vc110")
else()
	set(MSVC_LIB_PREFIX "")
endif()

#rc
if (NOT VERSION)
	if (METADATA_VERSION)
		set(VERSION ${METADATA_VERSION})
	else()
		set(VERSION "1.0.0.0")
	endif()
endif()
set(METADATA_VERSION ${VERSION} CACHE STRING "Version number for executable metadata" FORCE)

if(WINDOWS)
	if (NOT METADATA_COMPANY)
		set(METADATA_COMPANY "Crytek GmbH")
	endif()
	set(METADATA_COMPANY "${METADATA_COMPANY}" CACHE STRING "Company name for executable metadata")

	if (NOT METADATA_COPYRIGHT)
		string(TIMESTAMP year "%Y")
		set(METADATA_COPYRIGHT "(C) ${year} ${METADATA_COMPANY}")
	endif()
	set(METADATA_COPYRIGHT "${METADATA_COPYRIGHT}" CACHE STRING "Copyright string for executable metadata")	

	string(REPLACE . , METADATA_VERSION_COMMA ${METADATA_VERSION})
	set(METADATA_VERSION_COMMA ${METADATA_VERSION_COMMA} CACHE INTERNAL "" FORCE)
endif(WINDOWS)

include("${TOOLS_CMAKE_DIR}/Build.cmake")
