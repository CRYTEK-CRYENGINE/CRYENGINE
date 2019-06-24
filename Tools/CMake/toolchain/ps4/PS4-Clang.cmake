set(CMAKE_CROSSCOMPILING "TRUE")

# Use this variable to test if it is PlayStation 4 build in Cmake files
set(ORBIS 1)
# Unset WIN32 define
unset(WIN32)

if(${CMAKE_GENERATOR} MATCHES "Visual Studio")
	set(MSVC TRUE CACHE BOOL "Visual Studio Compiler" FORCE INTERNAL)
else()
	set(CLANG TRUE CACHE BOOL "Clang Compiler" FORCE INTERNAL)
	set(CMAKE_SKIP_RPATH TRUE CACHE BOOL "No RPATH on PlayStation 4 when compiling with Ninja")
endif()

set(OUTPUT_DIRECTORY_NAME "orbis")

get_filename_component(CRYENGINE_DIR     "${CMAKE_CURRENT_LIST_DIR}/../../../.."   REALPATH)
get_filename_component(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../modules" REALPATH)

message(STATUS "Crosscompiling for ORBIS")

set(CMAKE_SYSTEM           "ORBIS")
set(CMAKE_SYSTEM_NAME      "ORBIS")
set(CMAKE_SYSTEM_VERSION "1")
set(CMAKE_SYSTEM_PROCESSOR "AMD64")

if (MSVC)
    set(CMAKE_GENERATOR_PLATFORM "ORBIS")
    set(CMAKE_GENERATOR_TOOLSET "Clang")
endif()
if (EXISTS "${CRYENGINE_DIR}/Code/SDKs/Orbis")
    set(SCE_ORBIS_SDK_DIR "${CRYENGINE_DIR}/Code/SDKs/Orbis")
    message("Using bootstrapped PlayStation 4 SDK: ${SCE_ORBIS_SDK_DIR}.")
else()
    message("Using installed PlayStation 4 SDK: $ENV{SCE_ORBIS_SDK_DIR}")
    # Replace \\ to / in Windows Environment variable Orbis SDK
    string (REPLACE "\\" "/" SCE_ORBIS_SDK_DIR "$ENV{SCE_ORBIS_SDK_DIR}")
endif()

set(CMAKE_CONFIGURATION_TYPES Debug Profile Release)
set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "Reset the configurations to what we need" FORCE)

set(BUILD_CPU_ARCHITECTURE x64)
set(BUILD_PLATFORM ORBIS)

#######################################################################
# C++ Compiler
#######################################################################
set(CMAKE_C_COMPILER   "${SCE_ORBIS_SDK_DIR}/host_tools/bin/orbis-clang.exe"    CACHE INTERNAL "C Compiler")
set(CMAKE_CXX_COMPILER "${SCE_ORBIS_SDK_DIR}/host_tools/bin/orbis-clang++.exe"  CACHE INTERNAL "C++ Compiler")
set(CMAKE_AR           "${SCE_ORBIS_SDK_DIR}/host_tools/bin/orbis-ar.exe"       CACHE INTERNAL "Archiver")
set(CMAKE_RANLIB       "${SCE_ORBIS_SDK_DIR}/host_tools/bin/orbis-ranlib.exe"   CACHE INTERNAL "Ranlib")
set(CMAKE_LINKER       "${SCE_ORBIS_SDK_DIR}/host_tools/bin/orbis-ld.exe"       CACHE INTERNAL "Linker")

set(CMAKE_VS_SDK_EXECUTABLE_DIRECTORIES "${SCE_ORBIS_SDK_DIR}/host_tools/bin;$(VCTargetsPath)/Platforms/ORBIS;$(PATH)")
set(CMAKE_VS_SDK_INCLUDE_DIRECTORIES    "${SCE_ORBIS_SDK_DIR}/host_tools/lib/clang/include;${SCE_ORBIS_SDK_DIR}/target/include;${SCE_ORBIS_SDK_DIR}/target/include_common;")

# Setup CLANG flags
include ("${CMAKE_CURRENT_LIST_DIR}/../../CRYENGINE-CLANG.cmake")
set (ORBIS_COMMON_CLANG_FLAGS "-DORBIS -D_ORBIS -D__ORBIS__")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ORBIS_COMMON_CLANG_FLAGS} -D_LIB" CACHE STRING "C++ Common Flags" FORCE)
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${ORBIS_COMMON_CLANG_FLAGS}"          CACHE STRING "C Common Flags"   FORCE)

#######################################################################
# Linker flag
#######################################################################
set(CMAKE_EXE_LINKER_FLAGS_DEBUG   "" CACHE STRING "Additional Linker Debug Flags"   FORCE)
set(CMAKE_EXE_LINKER_FLAGS_PROFILE "" CACHE STRING "Additional Linker Profile Flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "" CACHE STRING "Additional Linker Release Flags" FORCE)
#######################################################################

set(CMAKE_CXX_COMPILER_ARG1 "")
set(CMAKE_CXX_PLATFORM_ID "ORBIS")
set(CMAKE_CXX_SIMULATE_ID "")
set(CMAKE_CXX_SIMULATE_VERSION "")
set(MSVC_CXX_ARCHITECTURE_ID x64)

# The default rules will select CMAKE_C_COMPILER / CMAKE_CXX_COMPILER when populating CMAKE_*_LINK_EXECUTABLE
# For compatibility with how SCE's MSBuild rules directly invoke the linker, we will do the same
# Note that this means that *_LINK_FLAGS cannot contain -Wl,*
set(CMAKE_C_LINKER "${CMAKE_LINKER}")
set(CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINKER} <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET>")
set(CMAKE_C_IGNORE_EXTENSIONS h;H;o;O;obj;OBJ;def;DEF;rc;RC)
set(CMAKE_C_SOURCE_FILE_EXTENSIONS c;m)

set(CMAKE_CXX_LINKER "${CMAKE_LINKER}")
set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINKER} <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET>")
set(CMAKE_CXX_COMPILER_LOADED 1)
set(CMAKE_CXX_COMPILER_WORKS TRUE)
set(CMAKE_CXX_ABI_COMPILED TRUE)
set(CMAKE_CXX_COMPILER_ENV_VAR "CXX")

set(CMAKE_COMPILER_IS_GNUCXX )
set(CMAKE_COMPILER_IS_MINGW )
set(CMAKE_COMPILER_IS_CYGWIN )
if(CMAKE_COMPILER_IS_CYGWIN)
	set(CYGWIN 1)
	set(UNIX 1)
endif()

if(CMAKE_COMPILER_IS_MINGW)
	set(MINGW 1)
endif()
set(CMAKE_CXX_COMPILER_ID_RUN 1)
set(CMAKE_CXX_IGNORE_EXTENSIONS inl;h;hpp;HPP;H;o;O;obj;OBJ;def;DEF;rc;RC)
set(CMAKE_CXX_SOURCE_FILE_EXTENSIONS C;M;c++;cc;cpp;cxx;m;mm;CPP)
set(CMAKE_CXX_LINKER_PREFERENCE 30)
set(CMAKE_CXX_LINKER_PREFERENCE_PROPAGATES 1)

# Save compiler ABI information.
set(CMAKE_CXX_SIZEOF_DATA_PTR "8")
set(CMAKE_CXX_COMPILER_ABI "")
set(CMAKE_CXX_LIBRARY_ARCHITECTURE "")

if(CMAKE_CXX_SIZEOF_DATA_PTR)
	set(CMAKE_SIZEOF_VOID_P "${CMAKE_CXX_SIZEOF_DATA_PTR}")
endif()

if(CMAKE_CXX_COMPILER_ABI)
	set(CMAKE_INTERNAL_PLATFORM_ABI "${CMAKE_CXX_COMPILER_ABI}")
endif()

if(CMAKE_CXX_LIBRARY_ARCHITECTURE)
	set(CMAKE_LIBRARY_ARCHITECTURE "")
endif()

set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")
set(CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES "")
set(CMAKE_CXX_IMPLICIT_LINK_FRAMEWORK_DIRECTORIES "")

# This macro should be included in every project to apply Visual Studio specific properties in the .vcxproj file
macro(set_target_properties_for_orbis Target)
	set_target_properties(${Target} PROPERTIES VS_GLOBAL_ApplicationEnvironment "title")
	set_target_properties(${Target} PROPERTIES VS_GLOBAL_DefaultLanguage "en-US")
	set_target_properties(${Target} PROPERTIES VS_GLOBAL_TargetRuntime "Native")
	set_target_properties(${Target} PROPERTIES VS_GLOBAL_ROOTNAMESPACE ${Target})
endmacro(set_target_properties_for_orbis)
