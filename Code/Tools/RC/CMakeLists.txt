cmake_minimum_required(VERSION 3.5)

enable_language(C)
enable_language(CXX)

set_property(GLOBAL PROPERTY DEBUG_CONFIGURATIONS Debug)

set(CMAKE_SYSTEM_VERSION 10.0.10586.0)

set(CMAKE_CONFIGURATION_TYPES Debug Release)
set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "Reset the configurations to what we need" FORCE)

set(BUILD_CPU_ARCHITECTURE x64)
set(BUILD_PLATFORM Win64)
set(WINDOWS 1)

set(CRYENGINE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../../..") 
set(OUTPUT_DIRECTORY "${CRYENGINE_SOURCE_DIR}/Tools/rc")

set(CMAKE_CXX_FLAGS "/W3 /fp:precise /MP /GF /Gy /DWIN32 /D_WINDOWS /GR- /Gd /Zm250 /Zc:forScope /wd4251 /wd4275 /utf-8 /Wv:18" CACHE STRING "C++ Flags" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG "/Zi /MTd /Od /Ob0 /Oy- /RTC1 /GS /bigobj /DFORCE_STANDARD_ASSERT /D_DEBUG" CACHE STRING "C++ Flags" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "/MT /Ox /Oi /Ot /Ob2 /Oy- /GS- /D NDEBUG /D_RELEASE /D PURE_CLIENT" CACHE STRING "C++ Flags" FORCE)

set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS}" CACHE STRING "C Flags" FORCE)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}" CACHE STRING "C Flags" FORCE)
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}" CACHE STRING "C Flags" FORCE)

project(ResourceCompiler${BUILD_PLATFORM} CXX C)

# SDK Directory
set(SDK_DIR "${CRYENGINE_SOURCE_DIR}/Code/SDKs")
set(CRY_LIBS_DIR "${CRYENGINE_SOURCE_DIR}/Code/Libs")
set(WINSDK_SDK_DIR "${CRYENGINE_SOURCE_DIR}/Code/SDKs/Microsoft Windows SDK")
set(WINSDK_SDK_LIB_DIR "${WINSDK_SDK_DIR}/V8.0/Lib/Win8/um")
set(WINSDK_SDK_INCLUDE_DIR "${WINSDK_SDK_DIR}/V8.0/Include/um")

if (EXISTS "${SDK_DIR}/POWERVR")
	option(OPTION_POWERVR "PVRTexTool support" ON)
endif()
if (EXISTS "${SDK_DIR}/astc-encoder")
	option(OPTION_ASTC "ASTC support" ON)
endif()

if(OPTION_POWERVR)
	add_definitions(/DTOOLS_SUPPORT_POWERVR)
endif() 
if(OPTION_ASTC)
	add_definitions(/DTOOLS_SUPPORT_ASTC)
endif() 

if (EXISTS "${SDK_DIR}/FbxSdk")
	option(OPTION_FBX_SDK "FBX SDK support" ON)
endif()
if (EXISTS "${SDK_DIR}/SubstanceEngines")
	option(OPTION_SUBSTANCE_SUPPORT "Support Substance Integration" ON)
else()
	unset(OPTION_SUBSTANCE_SUPPORT CACHE)
endif()
option(OPTION_PCH "Enable Precompiled Headers" OFF)

# Print current project settings
MESSAGE("CMAKE_SYSTEM_NAME = ${CMAKE_SYSTEM_NAME}")
MESSAGE("CMAKE_GENERATOR = ${CMAKE_GENERATOR}")
MESSAGE("CMAKE_CONFIGURATION_TYPES = ${CMAKE_CONFIGURATION_TYPES}")
MESSAGE("BUILD_PLATFORM = ${BUILD_PLATFORM}")
MESSAGE("OPTION_PCH = ${OPTION_PCH}")
MESSAGE("OPTION_FBX_SDK = ${OPTION_FBX_SDK}")
MESSAGE("OPTION_POWERVR = ${OPTION_POWERVR}")
MESSAGE("OPTION_SUBSTANCE_SUPPORT = ${OPTION_SUBSTANCE_SUPPORT}")
MESSAGE("OPTION_ASTC = ${OPTION_ASTC}")

set(RESOURCE_COMPILER TRUE)
set(BUILD_SHARED_LIBS TRUE)

include("${CMAKE_SOURCE_DIR}/RCCommonMacros.cmake")

if (EXISTS "${CMAKE_SOURCE_DIR}/XBoxOne.cmake")
	include("${CMAKE_SOURCE_DIR}/XBoxOne.cmake")
endif()

if (EXISTS "${CMAKE_SOURCE_DIR}/PS4.cmake")
	include("${CMAKE_SOURCE_DIR}/PS4.cmake")
endif()

include_directories( "${SDK_DIR}" )
include_directories( "${CRYENGINE_SOURCE_DIR}/Code/CryEngine/CryCommon" )

set(COMMON_LIBS Ntdll User32 Advapi32 Ntdll Ws2_32)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

#Libs
add_subdirectory("${CRY_LIBS_DIR}/expat" "${CMAKE_CURRENT_BINARY_DIR}/expat")
add_subdirectory("${CRY_LIBS_DIR}/lua" "${CMAKE_CURRENT_BINARY_DIR}/lua")
add_subdirectory("${CRY_LIBS_DIR}/zlib" "${CMAKE_CURRENT_BINARY_DIR}/zlib")
add_subdirectory("${CRY_LIBS_DIR}/lz4" "${CMAKE_CURRENT_BINARY_DIR}/lz4")	
add_subdirectory("${CRY_LIBS_DIR}/md5" "${CMAKE_CURRENT_BINARY_DIR}/md5")
add_subdirectory("${CRY_LIBS_DIR}/lzma" "${CMAKE_CURRENT_BINARY_DIR}/lzma")
add_subdirectory("${CRY_LIBS_DIR}/lzss" "${CMAKE_CURRENT_BINARY_DIR}/lzss")
add_subdirectory("${CRY_LIBS_DIR}/tiff" "${CMAKE_CURRENT_BINARY_DIR}/tiff")
if (EXISTS "${SDK_DIR}/szip" AND EXISTS "${CRY_LIBS_DIR}/szip")
	add_subdirectory("${CRY_LIBS_DIR}/szip" "${CMAKE_CURRENT_BINARY_DIR}/szip")
	set(HAVE_SZIP 1)
endif()
add_subdirectory("${CRY_LIBS_DIR}/ilmbase" "${CMAKE_CURRENT_BINARY_DIR}/ilmbase")
add_subdirectory("${CRY_LIBS_DIR}/alembic" "${CMAKE_CURRENT_BINARY_DIR}/alembic")
add_subdirectory("${CRY_LIBS_DIR}/hdf5" "${CMAKE_CURRENT_BINARY_DIR}/hdf5")
add_subdirectory("${CRY_LIBS_DIR}/vco" "${CMAKE_CURRENT_BINARY_DIR}/vco")
add_subdirectory("${CRY_LIBS_DIR}/qpOASES" "${CMAKE_CURRENT_BINARY_DIR}/qpOASES")

add_subdirectory("${CRYENGINE_SOURCE_DIR}/Code/Tools/CryXML" "${CMAKE_CURRENT_BINARY_DIR}/CryXML")
add_subdirectory("${CRYENGINE_SOURCE_DIR}/Code/CryEngine/CryPhysics" "${CMAKE_CURRENT_BINARY_DIR}/CryPhysics")
# ResourceCompiler
add_subdirectory(ResourceCompiler)
add_subdirectory(ResourceCompilerABC)
add_subdirectory(ResourceCompilerImage)
add_subdirectory(ResourceCompilerPC)
add_subdirectory(ResourceCompilerXML)
if (OPTION_SUBSTANCE_SUPPORT)
	add_subdirectory("${CRYENGINE_SOURCE_DIR}/Code/Sandbox/Libs/CrySubstance" "${CMAKE_CURRENT_BINARY_DIR}/CrySubstance")
	add_subdirectory(ResourceCompilerSubstance)
endif()
