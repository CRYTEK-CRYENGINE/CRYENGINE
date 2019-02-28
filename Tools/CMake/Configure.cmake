#Configures CRYENGINE CMake settings

cmake_minimum_required(VERSION 3.6.2)

set_property(GLOBAL PROPERTY DEBUG_CONFIGURATIONS Debug Profile)

# Turn on the ability to create folders to organize projects (.vcproj)
# It creates "CMakePredefinedTargets" folder by default and adds CMake
# defined projects like INSTALL.vcproj and ZERO_CHECK.vcproj
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if (NOT DEFINED CRYENGINE_DIR)
	set (CRYENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

if (NOT DEFINED PROJECT_DIR)
	set ( PROJECT_DIR "${CMAKE_SOURCE_DIR}" )
endif()
	
set( TOOLS_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}" )
	
#Fix slashes on paths
string(REPLACE "\\" "/" CRYENGINE_DIR "${CRYENGINE_DIR}")
string(REPLACE "\\" "/" TOOLS_CMAKE_DIR "${TOOLS_CMAKE_DIR}")

set(CMAKE_MODULE_PATH "${TOOLS_CMAKE_DIR}/modules")

# C/C++ languages required.
enable_language(C)
enable_language(CXX)
# Force C++11 support requirement
set (CMAKE_CXX_STANDARD 11)

if (DURANGO OR ORBIS OR ANDROID OR LINUX)
	unset(WIN32)
	unset(WIN64)
endif ()

if (WIN32)  # Either Win32 or Win64
	set(WINDOWS TRUE)
	include("${TOOLS_CMAKE_DIR}/toolchain/windows/WindowsPC-MSVC.cmake")
endif(WIN32)

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
	unset(WIN64)
endif ()

set(game_folder CACHE INTERNAL "Game folder used for resource files on Windows" FORCE)

if(LINUX32 OR LINUX64)
	set(LINUX TRUE)
endif()

# Define Options
get_property(global_defines DIRECTORY "${CRYENGINE_DIR}" PROPERTY COMPILE_DEFINITIONS)
get_property(global_includes DIRECTORY "${CRYENGINE_DIR}" PROPERTY INCLUDE_DIRECTORIES)
get_property(global_links DIRECTORY "${CRYENGINE_DIR}" PROPERTY LINK_DIRECTORIES)

# Print current project settings
MESSAGE(STATUS "CMAKE_SYSTEM_NAME = ${CMAKE_SYSTEM_NAME}")
MESSAGE(STATUS "CMAKE_GENERATOR = ${CMAKE_GENERATOR}")
MESSAGE(STATUS "CMAKE_CONFIGURATION_TYPES = ${CMAKE_CONFIGURATION_TYPES}")
MESSAGE(STATUS "BUILD_PLATFORM = ${BUILD_PLATFORM}")
MESSAGE(STATUS "OPTION_PROFILE = ${OPTION_PROFILE}")
MESSAGE(STATUS "OPTION_PCH = ${OPTION_PCH}")
MESSAGE(STATUS "MSVC = ${MSVC}")
MESSAGE(STATUS "CRYENGINE_DIR = ${CRYENGINE_DIR}")

if (OPTION_PROFILE)
	set_property( DIRECTORY PROPERTY COMPILE_DEFINITIONS $<$<CONFIG:Release>:_PROFILE> )
else()
	set_property( DIRECTORY PROPERTY COMPILE_DEFINITIONS $<$<CONFIG:Release>:_RELEASE> )
endif()

if(OPTION_UNITY_BUILD)
	message(STATUS "UNITY BUILD Enabled")
endif()

# SDK Directory
set(SDK_DIR "${CRYENGINE_DIR}/Code/SDKs")
set(CRY_LIBS_DIR "${CRYENGINE_DIR}/Code/Libs")
set(CRY_EXTENSIONS_DIR "${CRYENGINE_DIR}/Code/CryExtensions")

set(WINSDK_SDK_DIR "${SDK_DIR}/Microsoft Windows SDK")
set(WINSDK_SDK_LIB_DIR "${WINSDK_SDK_DIR}/V8.0/Lib/Win8/um")
set(WINSDK_SDK_INCLUDE_DIR "${WINSDK_SDK_DIR}/V8.0/Include/um")

# custom defines
list(APPEND global_defines "CRYENGINE_DEFINE")

include("${TOOLS_CMAKE_DIR}/CommonOptions.cmake")

# Must be included after SDK_DIR definition
include("${TOOLS_CMAKE_DIR}/CopyFilesToBin.cmake")

if (DURANGO)
	if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
		MESSAGE(STATUS "OPTION_STATIC_LINKING required for this configuration")
		set(OPTION_STATIC_LINKING ON CACHE BOOL "Required for Release build." FORCE)
	endif()
	if(NOT OPTION_STATIC_LINKING)
		MESSAGE(STATUS "Disabling Release builds; OPTION_STATIC_LINKING required on this platform")
		set(CMAKE_CONFIGURATION_TYPES Debug Profile CACHE STRING "Reset the configurations to what we need" FORCE)
	endif()
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

# Bootstrap support
if(EXISTS "${TOOLS_CMAKE_DIR}/Bootstrap.cmake")
	include("${TOOLS_CMAKE_DIR}/Bootstrap.cmake")
	if(OPTION_AUTO_BOOTSTRAP)
		set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "bootstrap.dat")
	endif()
elseif(EXISTS "${TOOLS_CMAKE_DIR}/DownloadSDKs.cmake")
	include("${TOOLS_CMAKE_DIR}/DownloadSDKs.cmake")
endif()

include("${TOOLS_CMAKE_DIR}/ConfigureChecks.cmake")
include("${TOOLS_CMAKE_DIR}/CommonMacros.cmake")

include("${TOOLS_CMAKE_DIR}/Recode.cmake")

if(WIN32)
	# Common Libriries linked to all targets
	set(COMMON_LIBS Ntdll User32 Advapi32 Ntdll Ws2_32)

	if (EXISTS "${SDK_DIR}/GPA" AND OPTION_ENGINE)
		list(APPEND global_includes "${SDK_DIR}/GPA/include" )
		if (WIN64)
			list(APPEND global_links "${SDK_DIR}/GPA/lib64" )
		else()
			list(APPEND global_links "${SDK_DIR}/GPA/lib32" )
		endif()
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

if ((WIN32 OR WIN64) AND OPTION_ENABLE_BROFILER AND OPTION_ENGINE)
	list(APPEND global_defines USE_BROFILER)
	list(APPEND global_includes "${SDK_DIR}/Brofiler" )
	list(APPEND global_links "${SDK_DIR}/Brofiler" )
	if (WIN64)
		set(COMMON_LIBS ${COMMON_LIBS} ProfilerCore64)
	elseif(WIN32)
		set(COMMON_LIBS ${COMMON_LIBS} ProfilerCore32)
	endif()
endif()

if (OPTION_ENGINE)
	if(NOT TARGET SDL2)
		include("${TOOLS_CMAKE_DIR}/modules/SDL2.cmake")
	endif()
endif()
include("${TOOLS_CMAKE_DIR}/modules/Boost.cmake")
include("${TOOLS_CMAKE_DIR}/modules/ncurses.cmake")

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

if(WIN32)
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
endif(WIN32)
